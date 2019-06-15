#include "SharedResources.h"
#include "Resources.h"

#include <res/ResourceSystem.h>

namespace gfx
{

static SharedResources s_sharedResources;

void InitSharedResources()
{
    auto irradCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/BakeIrradianceMap.cs.cso");
    s_sharedResources.m_bakeIrradPso = gpu::CreateComputePSO(res::GetData(irradCs)->m_shader);

    auto equics = res::LoadResourceSync<gfx::ShaderResource>("shaders/EquirectToCubemap.cs.cso");
	s_sharedResources.m_equiRectToCubePso = gpu::CreateComputePSO(res::GetData(equics)->m_shader);
}

void ShutdownSharedResources()
{
    s_sharedResources = SharedResources{};
}

SharedResources const& GetSharedResources()
{
    return s_sharedResources;
}

}