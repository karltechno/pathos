#pragma once
#include <gpu/Types.h>

namespace gfx
{

struct ShaderResource
{
	gpu::ShaderHandle m_shader;
};

void RegisterResourceLoaders();

}