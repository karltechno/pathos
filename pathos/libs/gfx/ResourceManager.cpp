#include "ResourceManager.h"

#include <string>
#include <stddef.h>

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

static kt::Array<uint8_t> ReadEntireFile(char const* _path)
{
	kt::Array<uint8_t> ret;

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
			gpu::ShaderBytecode newBytecode;

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

		for (gfx::Material const& mat : s_state.m_materials)
		{
			uint32_t constexpr memcpySize = sizeof(shaderlib::MaterialData::baseColour)
				+ sizeof(shaderlib::MaterialData::roughness)
				+ sizeof(shaderlib::MaterialData::metalness)
				+ sizeof(shaderlib::MaterialData::alphaCutoff);

			static_assert(offsetof(gfx::Material::Params, m_baseColour)			==	offsetof(shaderlib::MaterialData, baseColour), "Material mismatch.");
			static_assert(offsetof(gfx::Material::Params, m_roughnessFactor)	==	offsetof(shaderlib::MaterialData, roughness), "Material mismatch.");
			static_assert(offsetof(gfx::Material::Params, m_metallicFactor)		==	offsetof(shaderlib::MaterialData, metalness), "Material mismatch.");
			static_assert(offsetof(gfx::Material::Params, m_alphaCutoff)		==	offsetof(shaderlib::MaterialData, alphaCutoff), "Material mismatch.");
			
			memcpy((void*)materialGpuPtr, &mat.m_params, memcpySize);
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
	ModelIdx const idx = ModelIdx(uint16_t(s_state.m_meshes.Size()));
	s_state.m_models.PushBack().LoadFromGLTF(_path);
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
	kt::FilePath pathSantize(_path);
	State::ShaderCache::Iterator it = s_state.m_shaderCache.Find(std::string(pathSantize.Data()));
	if (it != s_state.m_shaderCache.End())
	{
		return it->m_val;
	}

	// TODO: Temp allocator here.
	kt::Array<uint8_t> shaderData = ReadEntireFile(_path);

	if (shaderData.Size() == 0)
	{
		return gpu::ShaderHandle{};
	}

	gpu::ShaderBytecode bytecode;
	bytecode.m_data = shaderData.Data();
	bytecode.m_size = shaderData.Size();

	gpu::ShaderHandle const handle = gpu::CreateShader(_type, bytecode, _path);

	s_state.m_shaderCache.Insert(std::string(pathSantize.Data()), gpu::ShaderRef(handle));
	return handle;
}

} // namespace SceneResourceManager

} // namespace gfx