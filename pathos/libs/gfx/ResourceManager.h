#pragma once
#include <kt/Slice.h>

#include <gpu/Types.h>
#include <shaderlib/CommonShared.h>

#include "Texture.h"
#include "Utils.h"

namespace gfx
{

struct Mesh;
struct Model;
struct Texture;
struct Material;
struct TangentSpace;

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
	// EnvMap
	gpu::PSORef m_bakeIrradPso;
	gpu::PSORef m_bakeGgxPso;
	gpu::PSORef m_equiRectToCubePso;
	gpu::TextureRef m_ggxLut;

	// Copying
	gpu::PSORef m_copyTexturePso;
	gpu::PSORef m_copyTextureArrayPso;

	// Generic textures
	TextureIdx m_texBlackIdx;
	TextureIdx m_texWhiteIdx;


	// Culling
	gpu::PSORef m_cullSubmeshPso;
	gpu::PSORef m_clearDrawCountPso;
};

SharedResources const& GetSharedResources();

void Init();
void Shutdown();

struct UnifiedBuffers
{
	gfx::ResizableDynamicBufferT<shaderlib::GPUSubMeshData> m_submeshGpuBuf;
	uint32_t m_numSubMeshes = 0;

	gpu::BufferRef m_posVertexBuf;
	gpu::BufferRef m_tangentSpaceVertexBuf;
	gpu::BufferRef m_uv0VertexBuf;
	gpu::BufferRef m_indexBufferRef;

	uint32_t m_vertexCapacity;
	uint32_t m_indexCapacity;

	uint32_t m_vertexUsed;
	uint32_t m_indexUsed;
};

// All models load vertex/index data into unified buffers. This is mainly for easy experimenting with GPU culling. 
// (Eg. ExecuteIndirect without needing to rebind separate vertex/index buffers).
// An alternative approach may be to allocate buffers with CreatedPlacedResource and alias them with one larger buffer.
void InitUnifiedBuffers(uint32_t _vertexCapacity = 2500000, uint32_t _indexCapacity = 2000000);
UnifiedBuffers const& GetUnifiedBuffers();

// A global R32_UINT buffer for use as UAV counters. Use AllocateCounterBufferIndex to allocate an index inside of it.
gpu::BufferHandle GetCounterBuffer();
uint32_t AllocateCounterBufferIndex();

void WriteIntoUnifiedBuffers
(
	float const* _positions,
	float const* _uvs,
	gfx::TangentSpace const* _tangentSpace,
	uint32_t const* _indices,
	uint32_t _numVertices,
	uint32_t _numIndices,
	uint32_t* o_idxOffset,
	uint32_t* o_vtxOffset
);

void AddSubMeshGPUData(gfx::Mesh& _model);

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

gpu::PersistentDescriptorTableHandle GetTextureDescriptorTable();

MaterialIdx CreateMaterial();
Material* GetMaterial(MaterialIdx _idx);
void SetMaterialsDirty();

gpu::BufferRef GetMaterialGpuBuffer();

gpu::ShaderHandle LoadShader(char const* _path, gpu::ShaderType _type);
}
}