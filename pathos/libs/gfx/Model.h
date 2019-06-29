#pragma once
#include <kt/kt.h>
#include <kt/Array.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>

#include <gpu/Types.h>
#include <gpu/HandleRef.h>

#include "Material.h"

namespace kt
{
struct IReader;
}

namespace gfx
{
// Todo: move to another file?
struct TangentSpace
{
	kt::Vec3 m_norm;
	kt::Vec4 m_tangentWithSign;
};


struct Model
{
	struct SubMesh
	{
		uint16_t m_materialIdx;

		uint32_t m_indexBufferStartOffset;
		uint32_t m_numIndicies;
	};

	KT_NO_COPY(Model);
	Model() = default;

	static void RegisterResourceLoader();

	static gpu::VertexLayout FullVertexLayout();
	static gpu::VertexLayout FullVertexLayoutInstanced();

	bool LoadFromGLTF(char const* _path);

	kt::Array<kt::Vec3> m_posStream;
	kt::Array<TangentSpace> m_tangentStream;
	kt::Array<kt::Vec2> m_uvStream0;
	kt::Array<uint32_t> m_colourStream;
	kt::Array<uint32_t> m_indicies;

	kt::Array<SubMesh> m_meshes;
	kt::Array<kt::AABB> m_meshBoundingBoxes;

	gpu::BufferRef m_posGpuBuf;
	gpu::BufferRef m_indexGpuBuf;
	gpu::BufferRef m_tangentGpuBuf;
	gpu::BufferRef m_uv0GpuBuf;

	kt::Array<Material> m_materials;

	kt::AABB m_boundingBox;

	// Index into global scene struct.
	uint32_t m_globalSceneIndex;
};


}