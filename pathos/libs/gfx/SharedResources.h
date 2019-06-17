#pragma once
#include <gpu/Types.h>
#include <gpu/HandleRef.h>

namespace gfx
{

struct SharedResources
{
    gpu::PSORef m_bakeIrradPso;
    gpu::PSORef m_equiRectToCubePso;

	gpu::TextureRef m_texBlack;
	gpu::TextureRef m_texWhite;
};

void InitSharedResources();
void ShutdownSharedResources();

SharedResources const& GetSharedResources();
}