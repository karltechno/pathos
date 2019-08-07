#include "Types.h"

// Must match order in Types.h

#define GPU_FMT_ONE(_pathos, _dxgi, _bitsPerPixel)
#define GPU_FMT_ALL \
	GPU_FMT_ONE(gpu::Format::Unknown,				DXGI_FORMAT_UNKNOWN,				0) \
	GPU_FMT_ONE(gpu::Format::R8G8_UNorm,			DXGI_FORMAT_R8G8_UNORM,				16) \
	GPU_FMT_ONE(gpu::Format::R8G8_SNorm,			DXGI_FORMAT_R8G8_SNORM,				16) \
	GPU_FMT_ONE(gpu::Format::R8G8B8A8_UNorm,		DXGI_FORMAT_R8G8B8A8_UNORM,			32) \
	GPU_FMT_ONE(gpu::Format::R8G8B8A8_UNorm_SRGB,	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,	32) \
	GPU_FMT_ONE(gpu::Format::R8G8B8A8_SNorm,		DXGI_FORMAT_R8G8B8A8_SNORM,			32) \
	GPU_FMT_ONE(gpu::Format::R10G10B10A2_UNorm,		DXGI_FORMAT_R10G10B10A2_UNORM,		32)	\
	GPU_FMT_ONE(gpu::Format::R32_Float,				DXGI_FORMAT_R32_FLOAT,				32) \
	GPU_FMT_ONE(gpu::Format::R32G32_Float,			DXGI_FORMAT_R32G32_FLOAT,			64) \
	GPU_FMT_ONE(gpu::Format::R32G32B32_Float,		DXGI_FORMAT_R32G32B32_FLOAT,		96) \
	GPU_FMT_ONE(gpu::Format::R32G32B32A32_Float,	DXGI_FORMAT_R32G32B32A32_FLOAT,		128) \
	GPU_FMT_ONE(gpu::Format::R16_Float,				DXGI_FORMAT_R16_FLOAT,				16) \
	GPU_FMT_ONE(gpu::Format::R16B16_Float,			DXGI_FORMAT_R16G16_FLOAT,			32) \
	GPU_FMT_ONE(gpu::Format::R16B16G16A16_Float,	DXGI_FORMAT_R16G16B16A16_FLOAT,		64) \
	GPU_FMT_ONE(gpu::Format::R16_Uint,				DXGI_FORMAT_R16_UINT,				16) \
	GPU_FMT_ONE(gpu::Format::R32_Uint,				DXGI_FORMAT_R32_UINT,				32) \
	GPU_FMT_ONE(gpu::Format::D32_Float,				DXGI_FORMAT_D32_FLOAT,				32) \
		

#define GPU_BLENDMODE_ONE(_pathos, _d3d12)
#define GPU_BLENDMODE_ALL \
	GPU_BLENDMODE_ONE(gpu::BlendMode::Zero, D3D12_BLEND_ZERO) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::One, D3D12_BLEND_ONE) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::SrcColor, D3D12_BLEND_SRC_COLOR) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::InvSrcColor, D3D12_BLEND_INV_SRC_COLOR) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::SrcAlpha, D3D12_BLEND_SRC_ALPHA) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::InvSrcAlpha, D3D12_BLEND_INV_SRC_ALPHA) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::DestAlpha, D3D12_BLEND_DEST_ALPHA) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::InvDestAlpha, D3D12_BLEND_INV_DEST_ALPHA) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::DestColor, D3D12_BLEND_DEST_COLOR) \
	GPU_BLENDMODE_ONE(gpu::BlendMode::InvDestColor, D3D12_BLEND_INV_DEST_COLOR) 


#define GPU_BLENDOP_ONE(_pathos, _d3d12) 
#define GPU_BLENDOP_ALL \
	GPU_BLENDOP_ONE(gpu::BlendOp::Add, D3D12_BLEND_OP_ADD) \
	GPU_BLENDOP_ONE(gpu::BlendOp::Sub, D3D12_BLEND_OP_SUBTRACT) \
	GPU_BLENDOP_ONE(gpu::BlendOp::RevSub, D3D12_BLEND_OP_REV_SUBTRACT) \
	GPU_BLENDOP_ONE(gpu::BlendOp::Min, D3D12_BLEND_OP_MIN) \
	GPU_BLENDOP_ONE(gpu::BlendOp::Max, D3D12_BLEND_OP_MAX) 


#define GPU_CMPFN_ONE(_pathos, _d3d12) 
#define GPU_CMPFN_ALL \
	GPU_CMPFN_ONE(gpu::ComparisonFn::Never, D3D12_COMPARISON_FUNC_NEVER) \
	GPU_CMPFN_ONE(gpu::ComparisonFn::Less, D3D12_COMPARISON_FUNC_LESS) \
	GPU_CMPFN_ONE(gpu::ComparisonFn::Equal, D3D12_COMPARISON_FUNC_EQUAL) \
	GPU_CMPFN_ONE(gpu::ComparisonFn::LessEqual, D3D12_COMPARISON_FUNC_LESS_EQUAL) \
	GPU_CMPFN_ONE(gpu::ComparisonFn::Greater, D3D12_COMPARISON_FUNC_GREATER) \
	GPU_CMPFN_ONE(gpu::ComparisonFn::NotEqual, D3D12_COMPARISON_FUNC_NOT_EQUAL) \
	GPU_CMPFN_ONE(gpu::ComparisonFn::GreaterEqual, D3D12_COMPARISON_FUNC_GREATER_EQUAL) \
	GPU_CMPFN_ONE(gpu::ComparisonFn::Always, D3D12_COMPARISON_FUNC_ALWAYS) 

#define GPU_PRIMTYPE_ONE(_pathos, _d3d12) 
#define GPU_PRIMTYPE_ALL \
	GPU_PRIMTYPE_ONE(gpu::PrimitiveType::PointList, D3D_PRIMITIVE_TOPOLOGY_POINTLIST) \
	GPU_PRIMTYPE_ONE(gpu::PrimitiveType::LineList, D3D_PRIMITIVE_TOPOLOGY_LINELIST) \
	GPU_PRIMTYPE_ONE(gpu::PrimitiveType::LineStrip, D3D_PRIMITIVE_TOPOLOGY_LINESTRIP) \
	GPU_PRIMTYPE_ONE(gpu::PrimitiveType::TriangleList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) \
	GPU_PRIMTYPE_ONE(gpu::PrimitiveType::TriangleStrip, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP) 


#define GPU_SEMANTIC_ONE(_pathos, _strD3d) 
#define GPU_SEMANTIC_ALL \
	GPU_SEMANTIC_ONE(gpu::VertexSemantic::Position, "Position") \
	GPU_SEMANTIC_ONE(gpu::VertexSemantic::TexCoord, "Texcoord") \
	GPU_SEMANTIC_ONE(gpu::VertexSemantic::Normal, "Normal") \
	GPU_SEMANTIC_ONE(gpu::VertexSemantic::Tangent, "Tangent") \
	GPU_SEMANTIC_ONE(gpu::VertexSemantic::Bitangent, "Bitangent") \
	GPU_SEMANTIC_ONE(gpu::VertexSemantic::Color, "Color") 
