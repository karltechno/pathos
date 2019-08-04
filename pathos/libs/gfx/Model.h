#pragma once
#include <kt/kt.h>
#include <kt/Array.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include <kt/Strings.h>

#include <gpu/Types.h>
#include <gpu/HandleRef.h>

#include "Scene.h"

namespace kt
{
struct IReader;
}

namespace gfx
{

struct TangentSpace
{
	kt::Vec3 m_norm;
	kt::Vec4 m_tangentWithSign;
};

struct Mesh
{
	void CreateGPUBuffers(bool _keepDataOnCpu = false);

	struct SubMesh
	{
		ResourceManager::MaterialIdx m_materialIdx;

		uint32_t m_indexBufferStartOffset;
		uint32_t m_numIndices;
	};

	kt::AABB m_boundingBox;
	kt::String64 m_name;

	kt::Array<kt::Vec3> m_posStream;
	kt::Array<TangentSpace> m_tangentStream;
	kt::Array<kt::Vec2> m_uvStream0;
	kt::Array<uint32_t> m_colourStream;
	kt::Array<uint32_t> m_indices;

	kt::Array<SubMesh> m_subMeshes;
	kt::Array<kt::AABB> m_subMeshBoundingBoxes;

	gpu::BufferRef m_posGpuBuf;
	gpu::BufferRef m_indexGpuBuf;
	gpu::BufferRef m_tangentGpuBuf;
	gpu::BufferRef m_uv0GpuBuf;
};

struct Model
{
	Model() = default;

	static gpu::VertexLayout FullVertexLayout();
	static gpu::VertexLayout FullVertexLayoutInstanced();

	bool LoadFromGLTF(char const* _path);

	std::string m_name;

	kt::Array<ResourceManager::MeshIdx> m_meshes;

	struct Node
	{
		kt::Mat4 m_mtx;
		uint32_t m_internalMeshIdx;
	};

	kt::Array<Node> m_nodes;

	kt::AABB m_boundingBox;
};


}