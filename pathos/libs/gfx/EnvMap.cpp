#include "EnvMap.h"
#include "Texture.h"
#include "SharedResources.h"

namespace gfx
{

void CreateCubemapFromEquirect(gpu::cmd::Context* _cmd, char const* _equirectPath, gpu::ResourceHandle _outCubemap)
{
	gfx::Texture equirectTex;
	if (!equirectTex.LoadFromFile(_equirectPath))
	{
		return;
	}

	CreateCubemapFromEquirect(_cmd, equirectTex.m_gpuTex, _outCubemap);
}

void CreateCubemapFromEquirect(gpu::cmd::Context* _cmd, gpu::ResourceHandle _inEquirect, gpu::ResourceHandle _outCubemap)
{
	gpu::ResourceType resType;
	gpu::TextureDesc equiDesc, cubeMapDesc;
	gpu::GetResourceInfo(_inEquirect, resType, nullptr, &equiDesc);
	KT_ASSERT(resType == gpu::ResourceType::Texture2D);
	gpu::GetResourceInfo(_outCubemap, resType, nullptr, &cubeMapDesc);
	KT_ASSERT(resType == gpu::ResourceType::TextureCube);

	gpu::cmd::SetPSO(_cmd, gfx::GetSharedResources().m_equiRectToCubePso);
	gpu::cmd::ResourceBarrier(_cmd, _outCubemap, gpu::ResourceState::UnorderedAccess);
	gpu::cmd::ResourceBarrier(_cmd, _inEquirect, gpu::ResourceState::ShaderResource);

	gpu::DescriptorData srv, uav;
	srv.Set(_inEquirect, 0);
	uav.Set(_outCubemap, 0);

	gpu::cmd::SetComputeSRVTable(_cmd, srv, 0);
	gpu::cmd::SetComputeUAVTable(_cmd, uav, 0);

	uint32_t constexpr c_equirectCsDim = 32;
	gpu::cmd::Dispatch(_cmd, cubeMapDesc.m_width / c_equirectCsDim, cubeMapDesc.m_height / c_equirectCsDim, 6);
}

void BakeEnvMapGGX(gpu::cmd::Context* _cmd, gpu::ResourceHandle _inCubeMap, gpu::ResourceHandle _outGGXMap)
{
	gpu::ResourceType resType;
	gpu::TextureDesc cubeDesc, ggxDesc;
	gpu::GetResourceInfo(_inCubeMap, resType, nullptr, &cubeDesc);
	KT_ASSERT(resType == gpu::ResourceType::TextureCube);
	gpu::GetResourceInfo(_outGGXMap, resType, nullptr, &ggxDesc);
	KT_ASSERT(resType == gpu::ResourceType::TextureCube);

	// TODO: could be smaller, just need to make a computer shader to downsample
	KT_ASSERT(cubeDesc.m_width == ggxDesc.m_width && cubeDesc.m_height == ggxDesc.m_height);

	gpu::cmd::ResourceBarrier(_cmd, _inCubeMap, gpu::ResourceState::ShaderResource);
	gpu::cmd::ResourceBarrier(_cmd, _outGGXMap, gpu::ResourceState::UnorderedAccess);

	// Copy top mip, todo: should probably make function for this pso.
	{
		gpu::cmd::SetPSO(_cmd, gfx::GetSharedResources().m_copyTextureArrayPso);


		gpu::DescriptorData srv;
		srv.SetSRVCubeAsArray(_inCubeMap);

		gpu::cmd::SetComputeSRVTable(_cmd, srv, 0);

		struct
		{
			uint32_t mip;
		} copyTexCbuf;

		copyTexCbuf.mip = 0;
		gpu::DescriptorData cbv;
		gpu::DescriptorData uav;
		cbv.Set(&copyTexCbuf, sizeof(copyTexCbuf));
		uav.Set(_outGGXMap, 0);

		gpu::cmd::SetComputeUAVTable(_cmd, uav, 0);
		gpu::cmd::SetComputeCBVTable(_cmd, cbv, 0);
		gpu::cmd::Dispatch(_cmd, ggxDesc.m_width / 8, ggxDesc.m_height / 8, 6);
	}

	gpu::cmd::SetPSO(_cmd, gfx::GetSharedResources().m_bakeGgxPso);

	struct  
	{
		kt::Vec2 inDimsMip0;
		kt::Vec2 outDims;
		float rough2;
	} cbuf;

	cbuf.inDimsMip0.x = float(cubeDesc.m_width);
	cbuf.inDimsMip0.y = float(cubeDesc.m_height);

	gpu::DescriptorData srv;
	srv.Set(_inCubeMap);

	gpu::cmd::SetComputeSRVTable(_cmd, srv, 0);

	for (uint32_t mip = 1; mip < ggxDesc.m_mipLevels; ++mip)
	{
		gpu::DescriptorData uav;
		uav.Set(_outGGXMap, mip);
		gpu::cmd::SetComputeUAVTable(_cmd, uav, 0);

		cbuf.outDims.x = float(kt::Max(1u, ggxDesc.m_width >> mip));
		cbuf.outDims.y = float(kt::Max(1u, ggxDesc.m_height >> mip));
		cbuf.rough2 = mip / float(ggxDesc.m_mipLevels);
		cbuf.rough2 *= cbuf.rough2;

		gpu::DescriptorData cbv;
		cbv.Set(&cbuf, sizeof(cbuf));
		gpu::cmd::SetComputeCBVTable(_cmd, cbv, 0);

		gpu::cmd::Dispatch(_cmd, uint32_t(kt::AlignUp(uint32_t(cbuf.outDims.x), 32)) / 32u, uint32_t(kt::AlignUp(uint32_t(cbuf.outDims.y), 32)) / 32u, 6);
	}

	gpu::cmd::ResourceBarrier(_cmd, _outGGXMap, gpu::ResourceState::ShaderResource);
}

}