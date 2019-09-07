#include "EnvMap.h"
#include "Texture.h"
#include "ResourceManager.h"
#include "Primitive.h"
#include "Camera.h"

#include <gpu/CommandContext.h>
#include <gpu/GPUDevice.h>

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
	GPU_PROFILE_SCOPE(_cmd, "EnvMap::CreateCubemapFromEquirect", GPU_PROFILE_COLOUR(0x00, 0xff, 0xff));

	gpu::ResourceType resType;
	gpu::TextureDesc equiDesc, cubeMapDesc;
	gpu::GetResourceInfo(_inEquirect, resType, nullptr, &equiDesc);
	KT_ASSERT(resType == gpu::ResourceType::Texture2D);
	gpu::GetResourceInfo(_outCubemap, resType, nullptr, &cubeMapDesc);
	KT_ASSERT(resType == gpu::ResourceType::TextureCube);

	gpu::cmd::SetPSO(_cmd, gfx::ResourceManager::GetSharedResources().m_equiRectToCubePso);
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
	GPU_PROFILE_SCOPE(_cmd, "EnvMap::BakeEnvMapGGX", GPU_PROFILE_COLOUR(0x00, 0x00, 0xff));

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
		gpu::cmd::SetPSO(_cmd, gfx::ResourceManager::GetSharedResources().m_copyTextureArrayPso);


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

	gpu::cmd::SetPSO(_cmd, gfx::ResourceManager::GetSharedResources().m_bakeGgxPso);

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

void SkyBoxRenderer::Init(gpu::ResourceHandle _cubeMap)
{
	m_cubemap = _cubeMap;

	gpu::ShaderRef const skyBoxVS = gfx::ResourceManager::LoadShader("shaders/SkyBox.vs.cso", gpu::ShaderType::Vertex);
	gpu::ShaderRef const skyBoxPS = gfx::ResourceManager::LoadShader("shaders/SkyBox.ps.cso", gpu::ShaderType::Pixel);

	gpu::GraphicsPSODesc skyBoxPso;
	skyBoxPso.m_depthStencilDesc.m_depthEnable = 1;
	skyBoxPso.m_depthStencilDesc.m_depthWrite = 0;
	skyBoxPso.m_depthStencilDesc.m_depthFn = gpu::ComparisonFn::LessEqual;
	skyBoxPso.m_rasterDesc.m_cullMode = gpu::CullMode::Front;
	skyBoxPso.m_depthFormat = gpu::BackbufferDepthFormat();
	skyBoxPso.m_numRenderTargets = 1;
	skyBoxPso.m_renderTargetFormats[0] = gpu::BackbufferFormat();
	skyBoxPso.m_vertexLayout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Position, false);
	skyBoxPso.m_vs = skyBoxVS;
	skyBoxPso.m_ps = skyBoxPS;
	m_skyBoxPso = gpu::CreateGraphicsPSO(skyBoxPso);

	gfx::PrimitiveBuffers buf;
	buf.m_genFlags = gfx::PrimitiveBuffers::GenFlags::PosOnly;
	gfx::GenCube(buf);
	m_primGpuBuf = gfx::MakePrimitiveGPUBuffers(buf);
}

void SkyBoxRenderer::Render(gpu::cmd::Context* _ctx, gfx::Camera const& _cam)
{
	GPU_PROFILE_SCOPE(_ctx, "Skybox Render", GPU_PROFILE_COLOUR(0xff, 0x00, 0xff));

	gpu::cmd::SetPSO(_ctx, m_skyBoxPso);
	gpu::DescriptorData srv;
	srv.Set(m_cubemap);

	gpu::cmd::SetGraphicsSRVTable(_ctx, srv, 0);
	gpu::cmd::SetVertexBuffer(_ctx, 0, m_primGpuBuf.m_pos);
	gpu::cmd::SetIndexBuffer(_ctx, m_primGpuBuf.m_indicies);
	kt::Mat4 skyMtx = _cam.GetView();

	skyMtx.SetPos(kt::Vec3(0.0f));
	skyMtx = _cam.GetProjection() * skyMtx;

	gpu::DescriptorData skyMtxDescriptor;
	skyMtxDescriptor.Set(&skyMtx, sizeof(skyMtx));

	gpu::cmd::SetGraphicsCBVTable(_ctx, skyMtxDescriptor, 0);

	gpu::cmd::SetViewportAndScissorRectFromTexture(_ctx, gpu::CurrentBackbuffer(), 1.0f, 1.0f);
	gpu::cmd::DrawIndexedInstanced(_ctx, m_primGpuBuf.m_numIndicies, 1, 0, 0, 0);

	gpu::cmd::ResetState(_ctx);
}

}