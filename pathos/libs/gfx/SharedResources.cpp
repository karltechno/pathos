#include "SharedResources.h"
#include "Resources.h"

#include <res/ResourceSystem.h>
#include <gpu/GPUDevice.h>
#include <gfx/Texture.h>

namespace gfx
{

static SharedResources s_sharedResources;

void InitSharedResources()
{
    auto irradCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/BakeIrradianceMap.cs.cso");
    s_sharedResources.m_bakeIrradPso = gpu::CreateComputePSO(res::GetData(irradCs)->m_shader, "Bake_Irradiance");

    auto equics = res::LoadResourceSync<gfx::ShaderResource>("shaders/EquirectToCubemap.cs.cso");
	s_sharedResources.m_equiRectToCubePso = gpu::CreateComputePSO(res::GetData(equics)->m_shader, "Equirect_To_CubeMap");

	{
		gpu::TextureDesc const texDesc = gpu::TextureDesc::Desc2D(4, 4, gpu::TextureUsageFlags::ShaderResource, gpu::Format::R8G8B8A8_UNorm);
		uint32_t texels[4*4];
		memset(texels, 0xFFFFFFFF, sizeof(texels));
		s_sharedResources.m_texWhite = gpu::CreateTexture(texDesc, texels, "White_Tex");
		memset(texels, 0x000000FF, sizeof(texels));
		s_sharedResources.m_texBlack = gpu::CreateTexture(texDesc, texels, "Black_Tex");
	}
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