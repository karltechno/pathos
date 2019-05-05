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

gpu::BlendDesc BlendDesc::Opaque()
{
	BlendDesc desc;

	desc.m_blendEnable = 0;
	desc.m_alphaToCoverageEnable = 0;

	desc.m_srcBlend = BlendMode::One;
	desc.m_destBlend = BlendMode::Zero;
	desc.m_blendOp = BlendOp::Add;

	desc.m_srcAlpha = BlendMode::One;
	desc.m_destAlpha = BlendMode::Zero;
	desc.m_blendOpAlpha = BlendOp::Add;
	return desc;
}


}