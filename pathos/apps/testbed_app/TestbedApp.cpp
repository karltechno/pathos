#include "TestbedApp.h"

#include <stdio.h>

#include <core/CVar.h>
#include <editor/Editor.h>
#include <gfx/SharedResources.h>
#include <gfx/Primitive.h>
#include <gfx/Model.h>
#include <gfx/EnvMap.h>
#include <gpu/GPUDevice.h>
#include <input/Input.h>

#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>

#include "imgui.h"
#include "gfx/Texture.h"

static core::CVar<float> s_camFov("cam.fov", "Camera field of view", 65.0f, 40.0f, 100.0f);
static core::CVar<bool> s_vsync("app.vsync", "Vsync enabled", true);

void TestbedApp::Setup()
{
	m_sceneWindow.SetScene(&m_scene);
	m_sceneWindow.SetMainViewCamera(&m_cam);

	m_pixelShader = res::LoadResourceSync<gfx::ShaderResource>("shaders/ObjectShader.ps.cso");
	m_vertexShader = res::LoadResourceSync<gfx::ShaderResource>("shaders/ObjectShader.vs.cso");

	{
		gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource;
		gpu::TextureDesc texDesc = gpu::TextureDesc::DescCube(1024, 1024, flags, gpu::Format::R32G32B32A32_Float);
		texDesc.m_mipLevels = gfx::MipChainLength(1024, 1024);
		m_cubeMap = gpu::CreateTexture(texDesc, nullptr, "CUBE_TEST");
	}

	{
		gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource;
		gpu::TextureDesc const texDesc = gpu::TextureDesc::DescCube(32, 32, flags, gpu::Format::R16B16G16A16_Float);
		m_irradMap = gpu::CreateTexture(texDesc, nullptr, "IRRAD_TEST");
	}

	{
		gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource;
		gpu::TextureDesc texDesc = gpu::TextureDesc::DescCube(1024, 1024, flags, gpu::Format::R16B16G16A16_Float);
		texDesc.m_mipLevels = gfx::MipChainLength(1024, 1024);
		m_ggxMap = gpu::CreateTexture(texDesc, nullptr, "GGX_MAP");
	}

	m_skyboxRenderer.Init(m_cubeMap);

	{
		gpu::GraphicsPSODesc psoDesc;
		psoDesc.m_depthFormat = gpu::BackbufferDepthFormat();
		psoDesc.m_numRenderTargets = 1;
		psoDesc.m_renderTargetFormats[0] = gpu::BackbufferFormat();
		psoDesc.m_vertexLayout = gfx::Model::FullVertexLayoutInstanced();
		psoDesc.m_vs = res::GetData(m_vertexShader)->m_shader;
		psoDesc.m_ps = res::GetData(m_pixelShader)->m_shader;

		m_pso = gpu::CreateGraphicsPSO(psoDesc);

		gpu::BufferDesc constantBufferDesc;
		constantBufferDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Transient;
		constantBufferDesc.m_sizeInBytes = sizeof(DummyCbuffer);
		m_constantBuffer = gpu::CreateBuffer(constantBufferDesc);

		//m_modelHandle = res::LoadResourceSync<gfx::Model>("models/DamagedHelmet/DamagedHelmet.gltf");
		//m_modelHandle = res::LoadResourceSync<gfx::Model>("models/sponza/Sponza.gltf");
		m_modelHandle = res::LoadResourceSync<gfx::Model>("models/MetalRoughSpheres/MetalRoughSpheres.gltf");

	}

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	//gfx::CreateCubemapFromEquirect(ctx, "textures/qwantani_2k.hdr", m_cubeMap);
	//gfx::CreateCubemapFromEquirect(ctx, "textures/Alexs_Apt_2k.hdr", m_cubeMap);
	//gfx::CreateCubemapFromEquirect(ctx, "textures/environment.hdr", m_cubeMap);
	gfx::CreateCubemapFromEquirect(ctx, "textures/cayley_interior_2k.hdr", m_cubeMap);
	
	gpu::GenerateMips(ctx, m_cubeMap);

	gfx::BakeEnvMapGGX(ctx, m_cubeMap, m_ggxMap);

	{
		gpu::cmd::SetPSO(ctx, gfx::GetSharedResources().m_bakeIrradPso);
		gpu::cmd::ResourceBarrier(ctx, m_irradMap, gpu::ResourceState::UnorderedAccess);

		gpu::DescriptorData srv;
		srv.Set(m_cubeMap);

		gpu::DescriptorData uav;
		uav.Set(m_irradMap);

		gpu::cmd::SetComputeSRVTable(ctx, srv, 0);
		gpu::cmd::SetComputeUAVTable(ctx, uav, 0);

		gpu::cmd::Dispatch(ctx, 32 / 32, 32 / 32, 6);
		gpu::cmd::ResourceBarrier(ctx, m_irradMap, gpu::ResourceState::ShaderResource);
	}
}

void TestbedApp::Tick(float _dt)
{
	gpu::SetVsyncEnabled(s_vsync);


	uint32_t swapchainW, swapchainH;
	gpu::GetSwapchainDimensions(swapchainW, swapchainH);

	gfx::Camera::ProjectionParams params;
	params.m_farPlane = 10000.0f;
	params.m_fov = kt::ToRadians(s_camFov);
	params.m_nearPlane = 1.0f;
	params.m_type = gfx::Camera::ProjType::Perspective;
	params.m_viewHeight = float(swapchainH);
	params.m_viewWidth = float(swapchainW);

	m_cam.SetProjection(params);

	m_camController.UpdateCamera(_dt, m_cam);
	m_myCbuffer.mvp = kt::Mat4::Identity();

	m_scene.BeginFrameAndUpdateBuffers(gpu::GetMainThreadCommandCtx(), m_cam, _dt);

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	gpu::TextureHandle backbuffer = gpu::CurrentBackbuffer();
	gpu::TextureHandle depth = gpu::BackbufferDepth();


	gpu::cmd::SetPSO(ctx, m_pso);
	gpu::cmd::UpdateTransientBuffer(ctx, m_constantBuffer, &m_myCbuffer, sizeof(m_myCbuffer));
	
	gpu::DescriptorData cbvs[2];
	cbvs[0].Set(m_constantBuffer);
	cbvs[1].Set(m_scene.m_frameConstantsGpuBuf);

	gpu::cmd::SetGraphicsCBVTable(ctx, cbvs[0], 0);
	gpu::cmd::SetGraphicsCBVTable(ctx, cbvs[1], 1);

	gpu::cmd::SetRenderTarget(ctx, 0, backbuffer);
	gpu::cmd::SetDepthBuffer(ctx, depth);


	float const col[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	gpu::cmd::ClearRenderTarget(ctx, backbuffer, col);
	gpu::cmd::ClearDepth(ctx, depth, 1.0f);

	gpu::DescriptorData envMaps[4];
	envMaps[0].Set(m_irradMap);
	envMaps[1].Set(m_ggxMap);
	envMaps[2].Set(gfx::GetSharedResources().m_ggxLut);
	envMaps[3].Set(m_scene.m_lightGpuBuf);;

	gpu::cmd::SetGraphicsSRVTable(ctx, envMaps, 1);
	m_scene.RenderInstances(ctx);

	m_skyboxRenderer.Render(ctx, m_cam);
	
	m_scene.EndFrame();
}

void TestbedApp::Shutdown()
{
}

void TestbedApp::HandleInputEvent(input::Event const& _ev)
{
	m_camController.DefaultInputHandler(_ev);
}

PATHOS_APP_IMPLEMENT_MAIN(TestbedApp);
