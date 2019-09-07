#include "ResourceManager.h"

#include <string>
#include <stddef.h>

#include <core/Memory.h>
#include <core/FolderWatcher.h>
#include <shaderlib/CommonShared.h>

#include <kt/Strings.h>
#include <kt/HashMap.h>
#include <kt/Hash.h>
#include <kt/Array.h>
#include <kt/FilePath.h>
#include <kt/Serialization.h>

#include "Model.h"
#include "Material.h"

namespace gfx
{

namespace ResourceManager
{

struct StdStringHashI
{
	using HashType = uint32_t;

	static bool Equal(std::string const& _lhs, std::string const& _rhs)
	{
		return kt::StrCmpI(_lhs.c_str(), _rhs.c_str()) == 0;
	}

	static HashType Hash(std::string const& _rhs)
	{
		return kt::StringHashI(_rhs.c_str());
	}
};

struct State
{
	static uint32_t constexpr c_maxBindlessTextures = 1024;

	using TextureCache = kt::HashMap<std::string, TextureIdx, StdStringHashI>;
	using ShaderCache = kt::HashMap<std::string, gpu::ShaderRef, StdStringHashI>;

	kt::Array<gfx::Mesh> m_meshes;
	kt::Array<gfx::Model> m_models;
	kt::Array<gfx::Texture> m_textures;
	kt::Array<gfx::Material> m_materials;

	gpu::PersistentDescriptorTableRef m_bindlessTextureHandle;

	SharedResources m_sharedResources;

	// TODO: Doesn't handle different texture load flags (unlikely to be an issue for now).
	TextureCache m_loadedTextureCache;
	ShaderCache m_shaderCache;

	gpu::BufferRef m_materialGpuBuf;

	UnifiedBuffers m_unifiedBuffers;

	core::FolderWatcher* m_shaderWatcher;

	bool m_materialsDirty = false;
} s_state;

static void CreateMaterialGpuBuffer(uint32_t _maxElems)
{
	gpu::BufferDesc materialBufferDesc;
	materialBufferDesc.m_strideInBytes = sizeof(shaderlib::MaterialData);
	materialBufferDesc.m_sizeInBytes = _maxElems * sizeof(shaderlib::MaterialData);
	materialBufferDesc.m_flags = gpu::BufferFlags::ShaderResource | gpu::BufferFlags::Dynamic;
	s_state.m_materialGpuBuf = gpu::CreateBuffer(materialBufferDesc, nullptr, "Material Buffer");
}

static void InitSharedResources()
{
	gpu::ShaderRef irradCs = ResourceManager::LoadShader("shaders/BakeIrradianceMap.cs.cso", gpu::ShaderType::Compute);
	s_state.m_sharedResources.m_bakeIrradPso = gpu::CreateComputePSO(irradCs, "Bake_Irradiance");

	gpu::ShaderRef ggxMapCs = ResourceManager::LoadShader("shaders/BakeEnvMapGGX.cs.cso", gpu::ShaderType::Compute);
	s_state.m_sharedResources.m_bakeGgxPso = gpu::CreateComputePSO(ggxMapCs, "Bake_GGX");

	gpu::ShaderRef equics = ResourceManager::LoadShader("shaders/EquirectToCubemap.cs.cso", gpu::ShaderType::Compute);
	s_state.m_sharedResources.m_equiRectToCubePso = gpu::CreateComputePSO(equics, "Equirect_To_CubeMap");

	gpu::ShaderRef copyTexCs = ResourceManager::LoadShader("shaders/CopyTexture.cs.cso", gpu::ShaderType::Compute);
	s_state.m_sharedResources.m_copyTexturePso = gpu::CreateComputePSO(copyTexCs, "Copy_Texture");

	gpu::ShaderRef copyTexArrayCs = ResourceManager::LoadShader("shaders/CopyTextureArray.cs.cso", gpu::ShaderType::Compute);
	s_state.m_sharedResources.m_copyTextureArrayPso = gpu::CreateComputePSO(copyTexArrayCs, "Copy_Texture_Array");

	{
		uint32_t const c_blackWhiteDim = 4;
		uint32_t texels[c_blackWhiteDim * c_blackWhiteDim];

		for (uint32_t& t : texels) { t = 0xFFFFFFFF; }
		s_state.m_sharedResources.m_texWhiteIdx = CreateTextureFromRGBA8((uint8_t const*)texels, c_blackWhiteDim, c_blackWhiteDim, TextureLoadFlags::None, "White_Tex");

		for (uint32_t& t : texels) { t = 0xFF000000; }
		s_state.m_sharedResources.m_texBlackIdx = CreateTextureFromRGBA8((uint8_t const*)texels, c_blackWhiteDim, c_blackWhiteDim, TextureLoadFlags::None, "Black_Tex");
	}

	{
		gpu::cmd::Context* cmdList = gpu::GetMainThreadCommandCtx();
		gpu::TextureDesc const desc = gpu::TextureDesc::Desc2D(256, 256, gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource, gpu::Format::R16B16_Float);
		s_state.m_sharedResources.m_ggxLut = gpu::CreateTexture(desc, nullptr, "GGX_BRDF_LUT");
		gpu::cmd::ResourceBarrier(cmdList, s_state.m_sharedResources.m_ggxLut, gpu::ResourceState::UnorderedAccess);

		gpu::ShaderRef ggxLutCs = ResourceManager::LoadShader("shaders/BakeGGXLut.cs.cso", gpu::ShaderType::Compute);
		gpu::PSORef ggxLutPso = gpu::CreateComputePSO(ggxLutCs, "GGX_Lut_Bake");

		gpu::cmd::SetPSO(cmdList, ggxLutPso);
		gpu::DescriptorData uav;
		uav.Set(s_state.m_sharedResources.m_ggxLut);
		gpu::cmd::SetComputeUAVTable(cmdList, uav, 0);
		gpu::cmd::Dispatch(cmdList, 256 / 8, 256 / 8, 1);
		gpu::cmd::ResourceBarrier(cmdList, s_state.m_sharedResources.m_ggxLut, gpu::ResourceState::ShaderResource);
	}
}

gfx::ResourceManager::SharedResources const& GetSharedResources()
{
	return s_state.m_sharedResources;
}

void Init()
{
	s_state.m_meshes.Reserve(1024);
	s_state.m_textures.Reserve(State::c_maxBindlessTextures);
	s_state.m_materials.Reserve(1024);
	s_state.m_loadedTextureCache.Reserve(512);

	s_state.m_bindlessTextureHandle = gpu::CreatePersistentDescriptorTable(State::c_maxBindlessTextures);

	CreateMaterialGpuBuffer(s_state.m_materials.Capacity());

	s_state.m_shaderWatcher = core::CreateFolderWatcher(kt::FilePath("shaders/"));

	InitSharedResources();
}

void Shutdown()
{
	s_state = State{};
}

void InitUnifiedBuffers(uint32_t _vertexCapacity /*= 2500000*/, uint32_t _indexCapacity /*= 2000000*/)
{
	s_state.m_unifiedBuffers.m_vertexCapacity = _vertexCapacity;
	s_state.m_unifiedBuffers.m_indexCapacity = _indexCapacity;
	s_state.m_unifiedBuffers.m_indexUsed = 0;
	s_state.m_unifiedBuffers.m_vertexUsed = 0;

	{
		gpu::BufferDesc indexDesc;
		indexDesc.m_flags = gpu::BufferFlags::Index | gpu::BufferFlags::Dynamic;
		indexDesc.m_format = gpu::Format::R32_Uint; // TODO: 16 bit
		indexDesc.m_sizeInBytes = sizeof(uint32_t) * _indexCapacity;
		indexDesc.m_strideInBytes = sizeof(uint32_t);
		s_state.m_unifiedBuffers.m_indexBufferRef = gpu::CreateBuffer(indexDesc, nullptr, "Unified Index Buffer");
	}

	{
		gpu::BufferDesc posDesc;
		posDesc.m_flags = gpu::BufferFlags::ShaderResource | gpu::BufferFlags::Dynamic;
		posDesc.m_sizeInBytes = sizeof(float[3]) * _vertexCapacity;
		posDesc.m_strideInBytes = sizeof(float[3]);
		s_state.m_unifiedBuffers.m_posVertexBuf = gpu::CreateBuffer(posDesc, nullptr, "Unified Vertex Buffer (Pos)");
	}

	{
		gpu::BufferDesc tangentSpaceDesc;
		tangentSpaceDesc.m_flags = gpu::BufferFlags::ShaderResource | gpu::BufferFlags::Dynamic;
		tangentSpaceDesc.m_sizeInBytes = sizeof(gfx::TangentSpace) * _vertexCapacity;
		tangentSpaceDesc.m_strideInBytes = sizeof(gfx::TangentSpace);
		s_state.m_unifiedBuffers.m_tangentSpaceVertexBuf = gpu::CreateBuffer(tangentSpaceDesc, nullptr, "Unified Vertex Buffer (Tangent Space)");
	}

	{
		gpu::BufferDesc uvDesc;
		uvDesc.m_flags = gpu::BufferFlags::ShaderResource | gpu::BufferFlags::Dynamic;
		uvDesc.m_sizeInBytes = sizeof(float[2]) * _vertexCapacity;
		uvDesc.m_strideInBytes = sizeof(float[2]);
		s_state.m_unifiedBuffers.m_uv0VertexBuf = gpu::CreateBuffer(uvDesc, nullptr, "Unified Vertex Buffer (UV0)");
	}
}

UnifiedBuffers const& GetUnifiedBuffers()
{
	return s_state.m_unifiedBuffers;
}

void WriteIntoUnifiedBuffers
(
	float const* _positions, 
	float const* _uvs, 
	gfx::TangentSpace const* _tangentSpace, 
	uint32_t const* _indices, 
	uint32_t _numVertices,
	uint32_t _numIndices, 
	uint32_t& o_idxOffset, 
	uint32_t& o_vtxOffset
)
{
	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	GPU_PROFILE_SCOPE(ctx, "ResourceManager::WriteIntoUnifiedBuffers", GPU_PROFILE_COLOUR(0xff, 0x00, 0xff));

	UnifiedBuffers& buffers = s_state.m_unifiedBuffers;

	KT_ASSERT(buffers.m_indexUsed + _numIndices < buffers.m_indexCapacity);
	KT_ASSERT(buffers.m_vertexUsed + _numVertices < buffers.m_vertexCapacity);
	o_idxOffset = buffers.m_indexUsed;
	o_vtxOffset = buffers.m_vertexUsed;

	gpu::cmd::UpdateDynamicBuffer(ctx, buffers.m_posVertexBuf, _positions, sizeof(float[3]) * _numVertices, buffers.m_vertexUsed * sizeof(float[3]));
	gpu::cmd::UpdateDynamicBuffer(ctx, buffers.m_uv0VertexBuf, _uvs, sizeof(float[2]) * _numVertices, buffers.m_vertexUsed * sizeof(float[2]));
	gpu::cmd::UpdateDynamicBuffer(ctx, buffers.m_tangentSpaceVertexBuf, _tangentSpace, sizeof(gfx::TangentSpace) * _numVertices, buffers.m_vertexUsed * sizeof(gfx::TangentSpace));
	gpu::cmd::UpdateDynamicBuffer(ctx, buffers.m_indexBufferRef, _indices, sizeof(uint32_t) * _numIndices, sizeof(uint32_t) * buffers.m_indexUsed); // TODO: 16 bit

	buffers.m_indexUsed += _numIndices;
	buffers.m_vertexUsed += _numVertices;
}

static kt::Array<uint8_t> ReadEntireFile(char const* _path)
{
	kt::Array<uint8_t> ret(core::GetThreadFrameAllocator());

	FILE* f = fopen(_path, "rb");
	if (!f)
	{
		KT_ASSERT(!"Failed to open file.");
		return ret;
	}
	KT_SCOPE_EXIT(fclose(f));

	kt::FileReader reader(f);
	uint8_t* mem = ret.PushBack_Raw(uint32_t(reader.OriginalSize()));
	
	reader.ReadBytes(mem, ret.Size());
	return ret;
}

void Update()
{
	core::UpdateFolderWatcher(s_state.m_shaderWatcher, [](char const* _changedPath)
	{
		kt::FilePath shaderBase("shaders/");
		shaderBase.Append(_changedPath);
		// TODO: bad allocations.
		State::ShaderCache::Iterator it = s_state.m_shaderCache.Find(std::string(shaderBase.Data()));
	
		if (it != s_state.m_shaderCache.End())
		{
			kt::Array<uint8_t> shaderData = ReadEntireFile(shaderBase.Data());

			if (shaderData.Size() == 0)
			{
				return;
			}

			gpu::ShaderBytecode bytecode;
			bytecode.m_data = shaderData.Data();
			bytecode.m_size = shaderData.Size();
			gpu::ReloadShader(it->m_val, bytecode);
		}
	});

	if (s_state.m_materialsDirty)
	{
		gpu::ResourceType ty;
		gpu::BufferDesc desc;
		gpu::GetResourceInfo(s_state.m_materialGpuBuf, ty, &desc);

		// ensure we have enough space.
		uint32_t const gpuElementMax = desc.m_sizeInBytes / sizeof(shaderlib::MaterialData);
		if (gpuElementMax < s_state.m_materials.Size())
		{
			CreateMaterialGpuBuffer(s_state.m_materials.Size() + s_state.m_materials.Size() / 4);
		}

		gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

		// TODO: We don't need to update everything if we just appended some materials, but whatever.

		uint32_t const updateSize = s_state.m_materials.Size() * sizeof(shaderlib::MaterialData);
		
		gpu::cmd::ResourceBarrier(ctx, s_state.m_materialGpuBuf, gpu::ResourceState::CopyDest);
		gpu::cmd::FlushBarriers(ctx);
		shaderlib::MaterialData* materialGpuPtr = (shaderlib::MaterialData*)gpu::cmd::BeginUpdateDynamicBuffer(ctx, s_state.m_materialGpuBuf, updateSize).Data();

		auto textureIdxOrDefault = [](gfx::ResourceManager::TextureIdx _myTexIdx, gfx::ResourceManager::TextureIdx _defaultTexIdx) -> uint32_t
		{
			return _myTexIdx.IsValid() ? uint32_t(_myTexIdx.idx) : uint32_t(_defaultTexIdx.idx);
		};

		for (gfx::Material const& mat : s_state.m_materials)
		{
			gfx::Material::Params const& params = mat.m_params;

			gfx::ResourceManager::SharedResources const& sharedRes = gfx::ResourceManager::GetSharedResources();

			static_assert(sizeof(materialGpuPtr->baseColour) == sizeof(mat.m_params.m_baseColour), "Mismatched colour size.");
			memcpy(&materialGpuPtr->baseColour, &params.m_baseColour, sizeof(kt::Vec4));
			materialGpuPtr->metalness = params.m_metallicFactor;
			materialGpuPtr->roughness = params.m_roughnessFactor;
			materialGpuPtr->alphaCutoff = params.m_alphaCutoff;
			materialGpuPtr->albedoTexIdx = textureIdxOrDefault(mat.m_textures[gfx::Material::TextureType::Albedo], sharedRes.m_texWhiteIdx);
			
			// TODO: Not a good default, need to handle in material shader anyway really.
			materialGpuPtr->normalMapTexIdx = textureIdxOrDefault(mat.m_textures[gfx::Material::TextureType::Normal], sharedRes.m_texWhiteIdx);
			materialGpuPtr->metalRoughTexIdx = textureIdxOrDefault(mat.m_textures[gfx::Material::TextureType::MetallicRoughness], sharedRes.m_texBlackIdx);
			materialGpuPtr->occlusionTexIdx = textureIdxOrDefault(mat.m_textures[gfx::Material::TextureType::Occlusion], sharedRes.m_texWhiteIdx);

			++materialGpuPtr;
		}

		gpu::cmd::EndUpdateDynamicBuffer(ctx, s_state.m_materialGpuBuf);
		gpu::cmd::ResourceBarrier(ctx, s_state.m_materialGpuBuf, gpu::ResourceState::ShaderResource);

		s_state.m_materialsDirty = false;
	}
}

MeshIdx CreateMesh()
{
	MeshIdx const idx = MeshIdx(uint16_t(s_state.m_meshes.Size()));
	s_state.m_meshes.PushBack();
	return idx;
}

gfx::Mesh* GetMesh(MeshIdx _idx)
{
	if (!_idx.IsValid())
	{
		return nullptr;
	}

	KT_ASSERT(_idx.idx < s_state.m_meshes.Size());
	return &s_state.m_meshes[_idx.idx];
}

ModelIdx CreateModel()
{
	ModelIdx const idx = ModelIdx(uint16_t(s_state.m_meshes.Size()));
	s_state.m_models.PushBack();
	return idx;
}

ModelIdx CreateModelFromGLTF(char const* _path)
{
	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	// For copying into unified buffers.
	{
		gpu::cmd::ResourceBarrier(ctx, s_state.m_unifiedBuffers.m_indexBufferRef, gpu::ResourceState::CopyDest);
		gpu::cmd::ResourceBarrier(ctx, s_state.m_unifiedBuffers.m_posVertexBuf, gpu::ResourceState::CopyDest);
		gpu::cmd::ResourceBarrier(ctx, s_state.m_unifiedBuffers.m_uv0VertexBuf, gpu::ResourceState::CopyDest);
		gpu::cmd::ResourceBarrier(ctx, s_state.m_unifiedBuffers.m_tangentSpaceVertexBuf, gpu::ResourceState::CopyDest);
		gpu::cmd::FlushBarriers(ctx);
	}

	ModelIdx const idx = ModelIdx(uint16_t(s_state.m_models.Size()));
	s_state.m_models.PushBack().LoadFromGLTF(_path);
	
	{
		gpu::cmd::ResourceBarrier(ctx, s_state.m_unifiedBuffers.m_indexBufferRef, gpu::ResourceState::IndexBuffer);
		gpu::cmd::ResourceBarrier(ctx, s_state.m_unifiedBuffers.m_posVertexBuf, gpu::ResourceState::ShaderResource);
		gpu::cmd::ResourceBarrier(ctx, s_state.m_unifiedBuffers.m_uv0VertexBuf, gpu::ResourceState::ShaderResource);
		gpu::cmd::ResourceBarrier(ctx, s_state.m_unifiedBuffers.m_tangentSpaceVertexBuf, gpu::ResourceState::ShaderResource);
		
		// TODO: Unecessary barriers if we load multiple models, unecessary flush. Barrier batching needs fixing.
		gpu::cmd::FlushBarriers(ctx);
	}

	return idx;
}

gfx::Model* GetModel(ModelIdx _idx)
{
	if (!_idx.IsValid())
	{
		return nullptr;
	}

	KT_ASSERT(_idx.idx < s_state.m_models.Size());
	return &s_state.m_models[_idx.idx];
}

kt::Slice<gfx::Model> GetAllModels()
{
	return kt::MakeSlice(s_state.m_models.Begin(), s_state.m_models.End());
}

kt::Slice<gfx::Mesh> GetAllMeshes()
{
	return kt::MakeSlice(s_state.m_meshes.Begin(), s_state.m_meshes.End());
}

kt::Slice<gfx::Material> GetAllMaterials()
{
	return kt::MakeSlice(s_state.m_materials.Begin(), s_state.m_materials.End());
}

TextureIdx CreateTextureFromFile(char const* _fileName, TextureLoadFlags _flags /*= TextureLoadFlags::None*/)
{
	// TODO: Unecessary string alloc/hash map lookup.
	State::TextureCache::Iterator it = s_state.m_loadedTextureCache.Find(std::string(_fileName));

	if (it != s_state.m_loadedTextureCache.End())
	{
		return it->m_val;
	}

	TextureIdx const idx = TextureIdx(uint16_t(s_state.m_textures.Size()));
	Texture& tex = s_state.m_textures.PushBack();
	tex.LoadFromFile(_fileName, _flags);

	gpu::SetPersistentTableSRV(s_state.m_bindlessTextureHandle, tex.m_gpuTex, idx.idx);

	kt::FilePath fp(_fileName);
	s_state.m_loadedTextureCache.Insert(std::string(fp.Data()), idx);
	return idx;
}

gfx::ResourceManager::TextureIdx CreateTextureFromRGBA8(uint8_t const* _texels, uint32_t _width, uint32_t _height, TextureLoadFlags _flags, char const* _debugName)
{
	TextureIdx const idx = TextureIdx(uint16_t(s_state.m_textures.Size()));
	Texture& tex = s_state.m_textures.PushBack();
	tex.LoadFromRGBA8(_texels, _width, _height, _flags, _debugName);
	gpu::SetPersistentTableSRV(s_state.m_bindlessTextureHandle, tex.m_gpuTex, idx.idx);
	return idx;
}

gfx::Texture* GetTexture(TextureIdx _idx)
{
	if (!_idx.IsValid())
	{
		return nullptr;
	}

	KT_ASSERT(_idx.idx < s_state.m_textures.Size());
	return &s_state.m_textures[_idx.idx];
}

gpu::PersistentDescriptorTableHandle GetTextureDescriptorTable()
{
	return s_state.m_bindlessTextureHandle;
}

MaterialIdx CreateMaterial()
{
	MaterialIdx const idx = MaterialIdx(uint16_t(s_state.m_materials.Size()));
	s_state.m_materials.PushBack();
	SetMaterialsDirty();
	return idx;
}

gfx::Material* GetMaterial(MaterialIdx _idx)
{
	if (!_idx.IsValid())
	{
		return nullptr;
	}

	KT_ASSERT(_idx.idx < s_state.m_materials.Size());
	return &s_state.m_materials[_idx.idx];
}

void SetMaterialsDirty()
{
	s_state.m_materialsDirty = true;
}

gpu::BufferRef GetMaterialGpuBuffer()
{
	return s_state.m_materialGpuBuf;
}

gpu::ShaderHandle LoadShader(char const* _path, gpu::ShaderType _type)
{
	// TODO: Unecessary string alloc/hash map lookup.
	kt::FilePath sanitizedPath(_path);
	State::ShaderCache::Iterator it = s_state.m_shaderCache.Find(std::string(sanitizedPath.Data()));
	if (it != s_state.m_shaderCache.End())
	{
		return it->m_val;
	}

	kt::Array<uint8_t> shaderData = ReadEntireFile(_path);

	if (shaderData.Size() == 0)
	{
		return gpu::ShaderHandle{};
	}

	gpu::ShaderBytecode bytecode;
	bytecode.m_data = shaderData.Data();
	bytecode.m_size = shaderData.Size();

	gpu::ShaderHandle const handle = gpu::CreateShader(_type, bytecode, _path);

	s_state.m_shaderCache.Insert(std::string(sanitizedPath.Data()), gpu::ShaderRef(handle));
	return handle;
}

} // namespace SceneResourceManager

} // namespace gfx