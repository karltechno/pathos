#pragma once
#include <gpu/Types.h>
#include <gpu/HandleRef.h>

namespace gfx
{

struct SharedResources
{
    gpu::PSORef m_bakeIrradPso;
    gpu::PSORef m_equiRectToCubePso;

	// First is linear, second srgb
	gpu::PSORef m_genMipsPso[2];
	gpu::PSORef m_genMipsArrayPso[2];
	gpu::PSORef m_genMipsCubePso[2];
};

void InitSharedResources();
void ShutdownSharedResources();

SharedResources const& GetSharedResources();

void GenerateMips(gpu::cmd::Context* _ctx, gpu::ResourceHandle _tex);
}