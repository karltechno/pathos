#include "Types.h"

namespace gpu
{


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

	desc.m_type = TextureType::Texture2D;
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
	
	desc.m_type = TextureType::Texture2D;
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

	desc.m_type = TextureType::Texture2D;
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

}