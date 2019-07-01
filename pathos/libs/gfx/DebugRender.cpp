#include "DebugRender.h"
#include "Resources.h"

#include <gpu/HandleRef.h>
#include <res/ResourceSystem.h>

#include <kt/Vec3.h>
#include <kt/Array.h>

namespace gfx
{

namespace DebugRender
{

// TODO: color struct perhaps?
static uint32_t PackColor(kt::Vec4 const& _col)
{
	uint32_t colu32 = 0;
	kt::Vec4 const c = kt::Clamp(_col, kt::Vec4(0.0f), kt::Vec4(1.0f)) * 255.0f;

	for (uint32_t i = 0; i < 4; ++i)
	{
		colu32 |= uint8_t(c[i] + 0.5f) << (i * 8);
	}

	return colu32;
}

struct LineVertex
{
	kt::Vec3 m_pos;
	uint32_t m_col;
};

struct State
{
	// First is no depth, second is depth.

	gpu::PSORef m_linePsos[2];
	kt::Array<LineVertex> m_lines[2];

	gpu::BufferRef m_lineTransientGpuBuf;

	LineVertex* FetchLineVertices(uint32_t _numVerticies, bool _depth)
	{
		return m_lines[_depth].PushBack_Raw(_numVerticies);
	}

} s_state;

void Init()
{
	{
		gpu::ShaderRef const vs = res::GetData(res::LoadResourceSync<gfx::ShaderResource>("shaders/DebugRender.vs.cso"))->m_shader;
		gpu::ShaderRef const ps = res::GetData(res::LoadResourceSync<gfx::ShaderResource>("shaders/DebugRender.ps.cso"))->m_shader;

		gpu::GraphicsPSODesc linePsoDesc;

		linePsoDesc.m_primType = gpu::PrimitiveType::LineList;
		linePsoDesc.m_numRenderTargets = 1;
		linePsoDesc.m_renderTargetFormats[0] = gpu::BackbufferFormat();
		linePsoDesc.m_depthFormat = gpu::BackbufferDepthFormat();
		linePsoDesc.m_vs = vs;
		linePsoDesc.m_ps = ps;

		linePsoDesc.m_vertexLayout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Position, false);
		linePsoDesc.m_vertexLayout.Add(gpu::Format::R8G8B8A8_UNorm, gpu::VertexSemantic::Color, false);

		s_state.m_linePsos[1] = gpu::CreateGraphicsPSO(linePsoDesc, "Debug Line Depth"); 

		linePsoDesc.m_depthStencilDesc.m_depthEnable = 0;
		linePsoDesc.m_depthStencilDesc.m_depthWrite = 0;

		s_state.m_linePsos[0] = gpu::CreateGraphicsPSO(linePsoDesc, "Debug Line No Depth");

		gpu::BufferDesc lineBufferDesc;
		lineBufferDesc.m_flags = gpu::BufferFlags::Transient | gpu::BufferFlags::Vertex;
		lineBufferDesc.m_strideInBytes = sizeof(LineVertex);
		s_state.m_lineTransientGpuBuf = gpu::CreateBuffer(lineBufferDesc, nullptr, "Debug Line Transient Buffer");
	}

}

void Shutdown()
{
	s_state = State{};
}

void Flush(gpu::cmd::Context* _ctx)
{
	for (uint32_t depth = 0; depth < 2; ++depth)
	{
		// TODO: should just write directly into mapped memory?
		uint32_t const numLineVtx = s_state.m_lines[depth].Size();
		if (numLineVtx)
		{
			LineVertex* vtx = s_state.m_lines[depth].Data();

			gpu::cmd::UpdateTransientBuffer(_ctx, s_state.m_lineTransientGpuBuf, vtx, numLineVtx * sizeof(LineVertex));

			gpu::cmd::SetPSO(_ctx, s_state.m_linePsos[depth]);
			gpu::cmd::SetVertexBuffer(_ctx, 0, s_state.m_lineTransientGpuBuf);
			gpu::cmd::DrawInstanced(_ctx, numLineVtx, 1, 0, 0);
			s_state.m_lines[depth].Clear();
		}
	}
}

void Line(kt::Vec3 const& _p0, kt::Vec3 const& _p1, kt::Vec4 const& _color, bool _depth)
{
	LineVertex* vtx = s_state.FetchLineVertices(2, _depth);

	vtx[0].m_pos = _p0;
	vtx[1].m_pos = _p1;

	vtx[0].m_col = vtx[1].m_col = PackColor(_color);
}

static void DoLineBoxFace(LineVertex* vertices, kt::Mat4 _mtx, uint32_t _col, uint32_t _face, float _sign)
{
	static kt::Vec2 const faceVerts2d[] = 
	{
		kt::Vec2(-1.0f, -1.0f),
		kt::Vec2(1.0f, -1.0f),
		kt::Vec2(1.0f, 1.0f),
		kt::Vec2(-1.0f, 1.0f),
	};

	kt::Vec3 verts[4];

	for (uint32_t vertIdx = 0; vertIdx < 4; ++vertIdx)
	{
		uint32_t vert2didx = 0;
		for (uint32_t component = 0; component < 3; ++component)
		{
			if (component == _face)
			{
				verts[vertIdx][component] = _sign;
			}
			else
			{
				verts[vertIdx][component] = faceVerts2d[vertIdx][vert2didx++];
			}
		}
		verts[vertIdx] = kt::MulPoint(_mtx, verts[vertIdx]);
	}

	for (uint32_t vertIdx = 0; vertIdx < 4; ++vertIdx)
	{
		vertices->m_col = _col;
		vertices->m_pos = verts[vertIdx];
		++vertices;
		vertices->m_col = _col;
		vertices->m_pos = verts[(vertIdx + 1) % 4];
		++vertices;
	}
}

void LineBox(kt::Mat4 const& _mat, kt::Vec4 const& _color, bool _depth)
{
	LineVertex* vtx = s_state.FetchLineVertices(8 * 6, _depth);
	uint32_t const col = PackColor(_color);

	for (uint32_t face = 0; face < 3; ++face)
	{
		DoLineBoxFace(vtx, _mat, col, face, -1.0f);
		vtx += 8;
		DoLineBoxFace(vtx, _mat, col, face, 1.0f);
		vtx += 8;
	}
}

void LineBox(kt::AABB const& _aabb, kt::Mat3 const& _mtx, kt::Vec3 const& _pos, kt::Vec4 const& _color, bool _depth)
{
	kt::Mat4 mtx(_mtx, _pos + kt::Mul(_mtx, _aabb.Center()));
	kt::Vec3 const scale = _aabb.HalfSize();
	
	mtx[0] *= scale.x;
	mtx[1] *= scale.y;
	mtx[2] *= scale.z;
	LineBox(mtx, _color, _depth);
}

}

}