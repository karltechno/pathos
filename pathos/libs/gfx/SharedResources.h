#pragma once
#include <gpu/Types.h>
#include <gpu/HandleRef.h>

namespace gfx
{

struct SharedResources
{
    gpu::PSORef m_bakeIrradPso;
    gpu::PSORef m_equiRectToCubePso;
};

void InitSharedResources();
void ShutdownSharedResources();

SharedResources const& GetSharedResources();

}