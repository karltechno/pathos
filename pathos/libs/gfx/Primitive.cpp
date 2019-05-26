#include "Primitive.h"

namespace gfx
{

void gfx::GenCube(PrimitiveBuffers& _buffers)
{
	static kt::Vec3 const c_faceNormals[] = 
	{
		{ 0.0f, 0.0f, 1.0f },	// +Z
		{ 0.0f, 0.0f, -1.0f },	// -Z
		{ 1.0f, 0.0f, 0.0f },	// +X
		{ -1.0f, 0.0f, 0.0f },	// -X
		{ 0.0f, 1.0f, 0.0f },	// +Y
		{ 0.0f, -1.0f, 0.0f }	// -Y
	};

	static kt::Vec2 const c_uvs[4] =
	{
		{1.0f, 0.0f},
		{1.0f, 1.0f},
		{0.0f, 1.0f},
		{0.0f, 0.0f}
	};
	
	for (uint32_t faceIdx = 0; faceIdx < 6; ++faceIdx)
	{
		// Find basis for this face.
		kt::Vec3 tangent;
		kt::Vec3 bitangent;

		if (faceIdx <= 1)
		{
			tangent = kt::Vec3(faceIdx == 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
			bitangent = kt::Vec3(0.0f, 1.0f, 0.0f);
		}
		else if (faceIdx <= 3)
		{
			tangent = kt::Vec3(0.0f, 0.0f, faceIdx == 2 ? -1.0f : 1.0f);
			bitangent = kt::Vec3(0.0f, 1.0f, 0.0f);
		}
		else
		{
			tangent = kt::Vec3(1.0f, 0.0f, 0.0f);
			bitangent = kt::Vec3(0.0f, 0.0f, faceIdx == 4 ? -1.0f : 1.0f);
		}

		uint16_t const idxStart = uint16_t(_buffers.m_pos.Size());
		uint16_t* indicies = _buffers.m_indicies.PushBack_Raw(6);
		*indicies++ = idxStart;
		*indicies++ = idxStart + 2;
		*indicies++ = idxStart + 1;

		*indicies++ = idxStart;
		*indicies++ = idxStart + 3;
		*indicies++ = idxStart + 2;

		kt::Vec3 const& norm = c_faceNormals[faceIdx];

		kt::Vec3* pos = _buffers.m_pos.PushBack_Raw(4);
		*pos++ = norm - tangent - bitangent;
		*pos++ = norm - tangent + bitangent;
		*pos++ = norm + tangent + bitangent;
		*pos++ = norm + tangent - bitangent;

		if (!!(_buffers.m_genFlags & PrimitiveBuffers::GenFlags::GenUVs))
		{
			memcpy(_buffers.m_uvs.PushBack_Raw(KT_ARRAY_COUNT(c_uvs)), c_uvs, sizeof(c_uvs));
		}

		if (!!(_buffers.m_genFlags & PrimitiveBuffers::GenFlags::GenTangentSpace))
		{
			TangentSpace* tangentSpace = _buffers.m_tangents.PushBack_Raw(4);
			kt::Vec4 const tangentWithSign(tangent, 1.0f);
			for (uint32_t i = 0; i < 4; ++i)
			{
				tangentSpace[i].m_norm = norm;
				tangentSpace[i].m_tangentWithSign = tangentWithSign;
			}
		}
	}
}

gfx::PrimitiveGPUBuffers MakePrimitiveGPUBuffers(PrimitiveBuffers const& _buffers)
{
	PrimitiveGPUBuffers ret;
	ret.m_numIndicies = _buffers.m_indicies.Size();
	
	{
		gpu::BufferDesc idxBufferDesc;
		idxBufferDesc.m_flags = gpu::BufferFlags::Index;
		idxBufferDesc.m_format = gpu::Format::R16_Uint;
		idxBufferDesc.m_strideInBytes = sizeof(uint16_t);
		idxBufferDesc.m_sizeInBytes = sizeof(uint16_t) * _buffers.m_indicies.Size();
		ret.m_indicies = gpu::CreateBuffer(idxBufferDesc, _buffers.m_indicies.Data(), "Primitive Idx Buf");
	}

	{
		gpu::BufferDesc posBufferDesc;
		posBufferDesc.m_flags = gpu::BufferFlags::Vertex;
		posBufferDesc.m_format = gpu::Format::R16_Uint;
		posBufferDesc.m_strideInBytes = sizeof(kt::Vec3);
		posBufferDesc.m_sizeInBytes = sizeof(kt::Vec3) * _buffers.m_pos.Size();
		ret.m_pos = gpu::CreateBuffer(posBufferDesc, _buffers.m_pos.Data(), "Primitive Pos Buf");
	}

	if(!!(_buffers.m_genFlags & PrimitiveBuffers::GenFlags::GenUVs))
	{
		gpu::BufferDesc uvBufferDesc;
		uvBufferDesc.m_flags = gpu::BufferFlags::Vertex;
		uvBufferDesc.m_strideInBytes = sizeof(kt::Vec2);
		uvBufferDesc.m_sizeInBytes = sizeof(kt::Vec2) * _buffers.m_uvs.Size();
		ret.m_uv = gpu::CreateBuffer(uvBufferDesc, _buffers.m_uvs.Data(), "Primitive uv Buf");
	}

	if (!!(_buffers.m_genFlags & PrimitiveBuffers::GenFlags::GenTangentSpace))
	{
		gpu::BufferDesc tangBufferDesc;
		tangBufferDesc.m_flags = gpu::BufferFlags::Vertex;
		tangBufferDesc.m_strideInBytes = sizeof(TangentSpace);
		tangBufferDesc.m_sizeInBytes = sizeof(TangentSpace) * _buffers.m_tangents.Size();
		ret.m_tangentSpace = gpu::CreateBuffer(tangBufferDesc, _buffers.m_tangents.Data(), "Primitive Tangent Buf");
	}

	return ret;
}


}