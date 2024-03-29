#include "Types.h"

#include "FormatConversion.h"

namespace gpu
{

#undef GPU_FMT_ONE
#define GPU_FMT_ONE(_1, _3, _size) _size,

static const uint32_t s_formatBits[] =
{
	GPU_FMT_ALL
};

#undef GPU_FMT_ONE
#define GPU_FMT_ONE(_fmt, ...) #_fmt,
static char const* const s_formatNames[] =
{
	GPU_FMT_ALL
};

gpu::TextureDesc TextureDesc::Desc1D(uint32_t _width, TextureUsageFlags _flags, Format _fmt)
{
	gpu::TextureDesc desc{};
	desc.m_depth = 1;
	desc.m_height = 1;
	desc.m_arraySlices = 1;
	desc.m_format = _fmt;
	desc.m_usageFlags = _flags;
	desc.m_format = _fmt;

	desc.m_width = _width;

	desc.m_type = ResourceType::Texture2D;
	return desc;
}

gpu::TextureDesc TextureDesc::Desc2D(uint32_t _width, uint32_t _height, TextureUsageFlags _flags, Format _fmt)
{
	gpu::TextureDesc desc{};
	desc.m_depth = 1;
	desc.m_arraySlices = 1;
	desc.m_format = _fmt;
	desc.m_usageFlags = _flags;
	desc.m_format = _fmt;
	
	desc.m_height = _height;
	desc.m_width = _width;
	
	desc.m_type = ResourceType::Texture2D;
	return desc;
}

gpu::TextureDesc TextureDesc::Desc3D(uint32_t _width, uint32_t _height, uint32_t _depth, TextureUsageFlags _flags, Format _fmt)
{
	gpu::TextureDesc desc{};
	desc.m_arraySlices = 1;
	desc.m_format = _fmt;
	desc.m_usageFlags = _flags;
	desc.m_format = _fmt;

	desc.m_height = _height;
	desc.m_width = _width;
	desc.m_depth = _depth;

	desc.m_type = ResourceType::Texture2D;
	return desc;
}

gpu::TextureDesc TextureDesc::DescCube(uint32_t _width, uint32_t _height, TextureUsageFlags _flags, Format _fmt)
{
	gpu::TextureDesc desc{};
	desc.m_arraySlices = 1;
	desc.m_format = _fmt;
	desc.m_usageFlags = _flags;
	desc.m_format = _fmt;

	desc.m_height = _height;
	desc.m_width = _width;

	desc.m_type = ResourceType::TextureCube;
	return desc;
}

bool IsDepthFormat(Format _fmt)
{
	switch (_fmt)
	{
		case gpu::Format::D32_Float:
		{
			return true;
		}	break;
	
		default:
		{
			return false;
		} break;

	}
}

uint32_t GetFormatSize(Format _fmt)
{
	return s_formatBits[uint32_t(_fmt)] / 8u;
}

char const* GetFormatName(Format _fmt)
{	
	// gpu::Format:: <- 13
	return s_formatNames[uint32_t(_fmt)] + 13;
}

bool IsSRGBFormat(Format _fmt)
{
	switch (_fmt)
	{
		case Format::R8G8B8A8_UNorm_SRGB:
			return true;

		default:
			return false;
	}
}

void BlendDesc::SetOpaque()
{	m_blendEnable = 0;
	m_alphaToCoverageEnable = 0;

	m_srcBlend = BlendMode::One;
	m_destBlend = BlendMode::Zero;
	m_blendOp = BlendOp::Add;

	m_srcAlpha = BlendMode::One;
	m_destAlpha = BlendMode::Zero;
	m_blendOpAlpha = BlendOp::Add;
}

}