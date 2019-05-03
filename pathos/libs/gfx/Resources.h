#pragma once
#include <gpu/Types.h>

#define PATHOS_PIXEL_SHADER_EXT		".ps"
#define PATHOS_VERTEX_SHADER_EXT	".vs"
#define PATHOS_COMPUTE_SHADER_EXT	".cs"

namespace gfx
{

struct ShaderResource
{
	gpu::ShaderHandle m_shader;
};

void RegisterResourceLoaders();

}