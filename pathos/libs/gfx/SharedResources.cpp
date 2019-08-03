#include "SharedResources.h"

#include <gpu/GPUDevice.h>
#include <gfx/Texture.h>

#include "Scene.h"

namespace gfx
{

static SharedResources s_sharedResources;

void InitSharedResources()
{
    gpu::ShaderRef irradCs = ResourceManager::LoadShader("shaders/BakeIrradianceMap.cs.cso", gpu::ShaderType::Compute);
    s_sharedResources.m_bakeIrradPso = gpu::CreateComputePSO(irradCs, "Bake_Irradiance");

	gpu::ShaderRef ggxMapCs = ResourceManager::LoadShader("shaders/BakeEnvMapGGX.cs.cso", gpu::ShaderType::Compute);
	s_sharedResources.m_bakeGgxPso = gpu::CreateComputePSO(ggxMapCs, "Bake_GGX");

	gpu::ShaderRef equics = ResourceManager::LoadShader("shaders/EquirectToCubemap.cs.cso", gpu::ShaderType::Compute);
	s_sharedResources.m_equiRectToCubePso = gpu::CreateComputePSO(equics, "Equirect_To_CubeMap");

	gpu::ShaderRef copyTexCs = ResourceManager::LoadShader("shaders/CopyTexture.cs.cso", gpu::ShaderType::Compute);
	s_sharedResources.m_copyTexturePso = gpu::CreateComputePSO(copyTexCs, "Copy_Texture");

	gpu::ShaderRef copyTexArrayCs = ResourceManager::LoadShader("shaders/CopyTextureArray.cs.cso", gpu::ShaderType::Compute);
	s_sharedResources.m_copyTextureArrayPso = gpu::CreateComputePSO(copyTexArrayCs, "Copy_Texture_Array");

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

		gpu::ShaderRef ggxLutCs = ResourceManager::LoadShader("shaders/BakeGGXLut.cs.cso", gpu::ShaderType::Compute);
		gpu::PSORef ggxLutPso = gpu::CreateComputePSO(ggxLutCs, "GGX_Lut_Bake");

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