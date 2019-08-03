#include "ResourceManager.h"

#include <string>

#include <core/FolderWatcher.h>

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
	using TextureCache = kt::HashMap<std::string, TextureIdx, StdStringHashI>;
	using ShaderCache = kt::HashMap<std::string, gpu::ShaderRef, StdStringHashI>;

	kt::Array<gfx::Mesh> m_meshes;
	kt::Array<gfx::Model> m_models;
	kt::Array<gfx::Texture> m_textures;
	kt::Array<gfx::Material> m_materials;

	// TODO: Doesn't handle different texture load flags (unlikely to be an issue for now).
	TextureCache m_loadedTextureCache;

	ShaderCache m_shaderCache;

	core::FolderWatcher* m_shaderWatcher;
} s_state;

void Init()
{
	s_state.m_meshes.Reserve(1024);
	s_state.m_textures.Reserve(1024);
	s_state.m_materials.Reserve(1024);
	s_state.m_loadedTextureCache.Reserve(512);

	s_state.m_shaderWatcher = core::CreateFolderWatcher(kt::FilePath("shaders/"));
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

void UpdateHotReload()
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

	kt::FilePath fp(_fileName);
	s_state.m_loadedTextureCache.Insert(std::string(fp.Data()), idx);
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