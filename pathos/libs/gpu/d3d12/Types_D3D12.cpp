#include <gpu/FormatConversion.h>
#include "Types_D3D12.h"

#include <kt/Macros.h>

#undef GPU_FMT_ONE
#define GPU_FMT_ONE(_pathos, _dxgi, _bpp) _dxgi,
static DXGI_FORMAT const s_toDxgi[] =
{
	GPU_FMT_ALL
};
static_assert(KT_ARRAY_COUNT(s_toDxgi) == uint32_t(gpu::Format::Num_Format), "Format array size mismatch");

#undef GPU_BLENDMODE_ONE
#define GPU_BLENDMODE_ONE(_pathos, _d3d) _d3d,
static D3D12_BLEND const s_toD3dBlend[] =
{
	GPU_BLENDMODE_ALL
};
static_assert(KT_ARRAY_COUNT(s_toD3dBlend) == uint32_t(gpu::BlendMode::Num_BlendMode), "BlendMode array size mismatch");

#undef GPU_BLENDOP_ONE
#define GPU_BLENDOP_ONE(_pathos, _d3d) _d3d,
static D3D12_BLEND_OP const s_toD3dBlendOp[] =
{
	GPU_BLENDOP_ALL
};
static_assert(KT_ARRAY_COUNT(s_toD3dBlendOp) == uint32_t(gpu::BlendOp::Num_BlendOp), "BlendOp array size mismatch");

#undef GPU_CMPFN_ONE
#define GPU_CMPFN_ONE(_pathos, _d3d) _d3d,
static D3D12_COMPARISON_FUNC const s_toD3dCmpFn[] =
{
	GPU_CMPFN_ALL
};
static_assert(KT_ARRAY_COUNT(s_toD3dCmpFn) == uint32_t(gpu::ComparisonFn::Num_ComparisonFn), "CmpFn array size mismatch");

#undef GPU_PRIMTYPE_ONE
#define GPU_PRIMTYPE_ONE(_path, _d3d) _d3d,
static D3D_PRIMITIVE_TOPOLOGY const s_toD3dPrimType[] =
{
	GPU_PRIMTYPE_ALL
};
static_assert(KT_ARRAY_COUNT(s_toD3dPrimType) == uint32_t(gpu::PrimitiveType::Num_PrimitiveType), "PrimType array size mismatch");

#undef GPU_SEMANTIC_ONE
#define GPU_SEMANTIC_ONE(_path, _d3d) _d3d,
static char const* const s_toD3dSemStr[] =
{
	GPU_SEMANTIC_ALL
};
static_assert(KT_ARRAY_COUNT(s_toD3dSemStr) == uint32_t(gpu::VertexSemantic::Num_VertexSemantic), "Semantic string array size mismatch");


namespace gpu
{

DXGI_FORMAT ToDXGIFormat(Format _fmt)
{
	return s_toDxgi[uint32_t(_fmt)];
}

D3D12_BLEND ToD3DBlend(gpu::BlendMode _mode)
{
	return s_toD3dBlend[uint32_t(_mode)];
}

D3D12_BLEND_OP ToD3DBlendOP(gpu::BlendOp _op)
{
	return s_toD3dBlendOp[uint32_t(_op)];
}

D3D12_COMPARISON_FUNC ToD3DCmpFn(ComparisonFn _fn)
{
	return s_toD3dCmpFn[uint32_t(_fn)];
}

D3D_PRIMITIVE_TOPOLOGY ToD3DPrimType(PrimitiveType _prim)
{
	return s_toD3dPrimType[uint32_t(_prim)];
}

char const* ToD3DSemanticStr(VertexSemantic _sem)
{
	return s_toD3dSemStr[uint32_t(_sem)];
}

D3D12_RESOURCE_STATES TranslateResourceState(ResourceState _state)
{
	switch (_state)
	{
		case ResourceState::Unknown: return D3D12_RESOURCE_STATE_COMMON;
		case ResourceState::Common: return D3D12_RESOURCE_STATE_COMMON;
		case ResourceState::RenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case ResourceState::DepthStencilTarget: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case ResourceState::DepthStencilTarget_ReadOnly: return D3D12_RESOURCE_STATE_DEPTH_READ;
		case ResourceState::ShaderResource_Read: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		case ResourceState::ShaderResource_ReadWrite: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		case ResourceState::CopyDest: return D3D12_RESOURCE_STATE_COPY_DEST;
		case ResourceState::CopySrc: return D3D12_RESOURCE_STATE_COPY_SOURCE;
		case ResourceState::Present: return D3D12_RESOURCE_STATE_PRESENT;
		case ResourceState::IndexBuffer: return D3D12_RESOURCE_STATE_INDEX_BUFFER;

		case ResourceState::VertexBuffer:
		case ResourceState::ConstantBuffer:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		default:
		case ResourceState::Num_ResourceState: KT_UNREACHABLE;
	}
}

}