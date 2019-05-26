#pragma once
#include <kt/Array.h>
#include <kt/Vec3.h>

#include <gfx/Model.h>

namespace gfx
{

struct PrimitiveBuffers
{
	enum class GenFlags : uint32_t
	{
		GenUVs,
		GenTangentSpace,
		Default = GenUVs | GenTangentSpace
	};

	kt::Array<kt::Vec3> m_pos;
	kt::Array<uint16_t> m_indicies;

	kt::Array<kt::Vec2> m_uvs;
	kt::Array<gfx::TangentSpace> m_tangents;

	GenFlags m_genFlags;
};

KT_ENUM_CLASS_FLAG_OPERATORS(PrimitiveBuffers::GenFlags);

struct PrimitiveGPUBuffers
{
	gpu::BufferRef m_pos;
	gpu::BufferRef m_indicies;
	gpu::BufferRef m_uv;
	gpu::BufferRef m_tangentSpace;

	uint32_t m_numIndicies = 0;
};

PrimitiveGPUBuffers MakePrimitiveGPUBuffers(PrimitiveBuffers const& _buffers);

void GenCube(PrimitiveBuffers& _buffers);


}