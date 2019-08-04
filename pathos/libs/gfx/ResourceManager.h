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

struct SharedResources
{
	gpu::PSORef m_bakeIrradPso;
	gpu::PSORef m_bakeGgxPso;
	gpu::PSORef m_equiRectToCubePso;

	gpu::PSORef m_copyTexturePso;
	gpu::PSORef m_copyTextureArrayPso;

	TextureIdx m_texBlackIdx;
	TextureIdx m_texWhiteIdx;

	gpu::TextureRef m_ggxLut;
};

SharedResources const& GetSharedResources();

void Init();
void Shutdown();

void Update();

MeshIdx CreateMesh();
gfx::Mesh* GetMesh(MeshIdx _idx);

ModelIdx CreateModel();
ModelIdx CreateModelFromGLTF(char const* _path);
gfx::Model* GetModel(ModelIdx _idx);

kt::Slice<gfx::Model> GetAllModels();
kt::Slice<gfx::Mesh> GetAllMeshes();
kt::Slice<gfx::Material> GetAllMaterials();

TextureIdx CreateTextureFromFile(char const* _fileName, TextureLoadFlags _flags = TextureLoadFlags::None);
TextureIdx CreateTextureFromRGBA8(uint8_t const* _texels, uint32_t _width, uint32_t _height, TextureLoadFlags _flags = TextureLoadFlags::None, char const* _debugName = nullptr);
gfx::Texture* GetTexture(TextureIdx _idx);

MaterialIdx CreateMaterial();
Material* GetMaterial(MaterialIdx _idx);
void SetMaterialsDirty();

gpu::BufferRef GetMaterialGpuBuffer();

gpu::ShaderHandle LoadShader(char const* _path, gpu::ShaderType _type);
}
}