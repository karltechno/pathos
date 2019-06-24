#pragma once
#include <gpu/Types.h>
#include <gpu/HandleRef.h>

namespace gfx
{

struct SharedResources
{
    gpu::PSORef m_bakeIrradPso;
	gpu::PSORef m_bakeGgxPso;
    gpu::PSORef m_equiRectToCubePso;

	gpu::PSORef m_copyTexturePso;
	gpu::PSORef m_copyTextureArrayPso;

	gpu::TextureRef m_texBlack;
	gpu::TextureRef m_texWhite;

	gpu::TextureRef m_ggxLut;
};

void InitSharedResources();
void ShutdownSharedResources();

SharedResources const& GetSharedResources();
}