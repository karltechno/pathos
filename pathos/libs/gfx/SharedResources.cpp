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

	auto ggxMapCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/BakeEnvMapGGX.cs.cso");
	s_sharedResources.m_bakeGgxPso = gpu::CreateComputePSO(res::GetData(ggxMapCs)->m_shader, "Bake_GGX");

    auto equics = res::LoadResourceSync<gfx::ShaderResource>("shaders/EquirectToCubemap.cs.cso");
	s_sharedResources.m_equiRectToCubePso = gpu::CreateComputePSO(res::GetData(equics)->m_shader, "Equirect_To_CubeMap");

	auto copyTexCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/CopyTexture.cs.cso");
	s_sharedResources.m_copyTexturePso = gpu::CreateComputePSO(res::GetData(copyTexCs)->m_shader, "Copy_Texture");

	auto copyTexArrayCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/CopyTextureArray.cs.cso");
	s_sharedResources.m_copyTextureArrayPso = gpu::CreateComputePSO(res::GetData(copyTexArrayCs)->m_shader, "Copy_Texture_Array");

	{
		gpu::TextureDesc const texDesc = gpu::TextureDesc::Desc2D(4, 4, gpu::TextureUsageFlags::ShaderResource, gpu::Format::R8G8B8A8_UNorm);
		uint32_t texels[4*4];
		for (uint32_t& t : texels) { t = 0xFFFFFFFF; }
		s_sharedResources.m_texWhite = gpu::CreateTexture(texDesc, texels, "White_Tex");
		for (uint32_t& t : texels) { t = 0xFF000000; }
		s_sharedResources.m_texBlack = gpu::CreateTexture(texDesc, texels, "Black_Tex");
	}

	{
		gpu::cmd::Context* cmdList = gpu::GetMainThreadCommandCtx();
		gpu::TextureDesc const desc = gpu::TextureDesc::Desc2D(256, 256, gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource, gpu::Format::R16B16_Float);
		s_sharedResources.m_ggxLut = gpu::CreateTexture(desc, nullptr, "GGX_BRDF_LUT");
		gpu::cmd::ResourceBarrier(cmdList, s_sharedResources.m_ggxLut, gpu::ResourceState::UnorderedAccess);

		auto ggxLutCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/BakeGGXLut.cs.cso");
		gpu::PSORef ggxLutPso = gpu::CreateComputePSO(res::GetData(ggxLutCs)->m_shader, "GGX_Lut_Bake");

		gpu::cmd::SetPSO(cmdList, ggxLutPso);
		gpu::DescriptorData uav;
		uav.Set(s_sharedResources.m_ggxLut);
		gpu::cmd::SetComputeUAVTable(cmdList, uav, 0);
		gpu::cmd::Dispatch(cmdList, 256 / 8, 256 / 8, 1);
		gpu::cmd::ResourceBarrier(cmdList, s_sharedResources.m_ggxLut, gpu::ResourceState::ShaderResource);
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