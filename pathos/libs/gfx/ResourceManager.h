#pragma once
#include <kt/Slice.h>

#include <gpu/Types.h>

#include "Texture.h"

namespace gfx
{

struct Mesh;
struct Model;
struct Texture;
struct Material;

namespace ResourceManager
{

template <typename T>
struct Index
{
	static uint16_t const c_invalidIdx = UINT16_MAX;

	Index() = default;
	explicit Index(uint16_t _idx)
		: idx(_idx)
	{
	}

	bool IsValid() const
	{
		return idx != c_invalidIdx;
	}

	uint16_t idx = c_invalidIdx;
};

template <typename T>
inline bool operator==(Index<T> _lhs, Index<T> _rhs)
{
	return _lhs.idx == _rhs.idx;
}

template <typename T>
inline bool operator!=(Index<T> _lhs, Index<T> _rhs)
{
	return _lhs.idx != _rhs.idx;
}

using MaterialIdx = Index<Material>;
using TextureIdx = Index<Texture>;
using MeshIdx = Index<Mesh>;
using ModelIdx = Index<Model>;

void Init();
void Shutdown();

void UpdateHotReload();

MeshIdx CreateMesh();
gfx::Mesh* GetMesh(MeshIdx _idx);

ModelIdx CreateModel();
ModelIdx CreateModelFromGLTF(char const* _path);
gfx::Model* GetModel(ModelIdx _idx);

kt::Slice<gfx::Model> GetAllModels();
kt::Slice<gfx::Mesh> GetAllMeshes();

TextureIdx CreateTextureFromFile(char const* _fileName, TextureLoadFlags _flags = TextureLoadFlags::None);
gfx::Texture* GetTexture(TextureIdx _idx);

MaterialIdx CreateMaterial();
Material* GetMaterial(MaterialIdx _idx);

gpu::ShaderHandle LoadShader(char const* _path, gpu::ShaderType _type);
}
}