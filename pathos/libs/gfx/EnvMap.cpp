#include "EnvMap.h"
#include "Texture.h"
#include "SharedResources.h"

namespace gfx
{

void CreateCubemapFromEquirect(char const* _equirectPath, gpu::ResourceHandle _outCubemap, gpu::cmd::Context* _cmd)
{
	gfx::Texture equirectTex;
	if (!equirectTex.LoadFromFile(_equirectPath))
	{
		return;
	}

	CreateCubemapFromEquirect(equirectTex.m_gpuTex, _outCubemap, _cmd);
}

void CreateCubemapFromEquirect(gpu::ResourceHandle _inEquirect, gpu::ResourceHandle _outCubemap, gpu::cmd::Context* _cmd)
{

	gpu::ResourceType resType;
	gpu::TextureDesc equiDesc, cubeMapDesc;
	gpu::GetResourceInfo(_inEquirect, resType, nullptr, &equiDesc);
	KT_ASSERT(resType == gpu::ResourceType::Texture2D);
	gpu::GetResourceInfo(_outCubemap, resType, nullptr, &cubeMapDesc);
	KT_ASSERT(resType == gpu::ResourceType::TextureCube);

	gpu::cmd::SetPSO(_cmd, gfx::GetSharedResources().m_equiRectToCubePso);
	gpu::cmd::ResourceBarrier(_cmd, _outCubemap, gpu::ResourceState::ShaderResource_ReadWrite);
	gpu::cmd::ResourceBarrier(_cmd, _inEquirect, gpu::ResourceState::ShaderResource_Read);

	gpu::DescriptorData srv, uav;
	srv.Set(_inEquirect, 0);
	uav.Set(_outCubemap, 0);

	gpu::cmd::SetComputeSRVTable(_cmd, srv, 0);
	gpu::cmd::SetComputeUAVTable(_cmd, uav, 0);

	uint32_t constexpr c_equirectCsDim = 32;
	gpu::cmd::Dispatch(_cmd, cubeMapDesc.m_width / c_equirectCsDim, cubeMapDesc.m_height / c_equirectCsDim, 6);
}

}