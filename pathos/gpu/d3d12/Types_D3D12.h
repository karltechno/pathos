#pragma once
#include <gpu/Types.h>

#include <d3d12.h>

namespace gpu
{

DXGI_FORMAT ToDXGIFormat(gpu::Format _fmt);
D3D12_BLEND ToD3DBlend(gpu::BlendMode _mode);
D3D12_BLEND_OP ToD3DBlendOP(gpu::BlendOp _op);
D3D12_COMPARISON_FUNC ToD3DCmpFn(gpu::ComparisonFn _fn);
D3D_PRIMITIVE_TOPOLOGY ToD3DPrimType(gpu::PrimitiveType _prim);
char const* ToD3DSemanticStr(gpu::VertexSemantic _sem);

constexpr uint32_t c_d3dBufferedFrames = 3u;


}