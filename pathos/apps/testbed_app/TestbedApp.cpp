#include "TestbedApp.h"

#include <stdio.h>

#include <core/CVar.h>
#include <editor/Editor.h>
#include <gfx/DebugRender.h>
#include <gfx/ResourceManager.h>
#include <gfx/Primitive.h>
#include <gfx/Model.h>
#include <gfx/EnvMap.h>
#include <gfx/Texture.h>
#include <gfx/ShadowUtils.h>
#include <gpu/GPUDevice.h>
#include <input/Input.h>
#include <shaderlib/DefinesShared.h>

#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>

#include "imgui.h"

static core::CVar<float> s_camFov("cam.fov", "Camera field of view", 65.0f, 40.0f, 100.0f);
static core::CVar<float> s_camNearPlane("cam.near_plane", "Camera near plane", 2.0f, 0.1f, 20.0f);
static core::CVar<float> s_camFarPlane("cam.far_plane", "Camera far plane", 5000.0f, 100.0f, 10000.0f);

static core::CVar<bool> s_vsync("app.vsync", "Vsync enabled", true);

constexpr uint32_t c_numShadowCascades = 4;

static const float c_shadowMapRes = 2048.0f;

void TestbedApp::Setup()
{
	gfx::ResourceManager::InitUnifiedBuffers();

	m_scene.Init(uint32_t(c_shadowMapRes));

	m_sceneWindow.SetScene(&m_scene);
	m_sceneWindow.SetMainViewCamera(&m_cam);

	gpu::ShaderRef const pixelShader = gfx::ResourceManager::LoadShader("shaders/ObjectShader.ps.cso", gpu::ShaderType::Pixel);
	gpu::ShaderRef const vertexShader = gfx::ResourceManager::LoadShader("shaders/ObjectShader.vs.cso", gpu::ShaderType::Vertex);

	{
		gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource;
		gpu::TextureDesc texDesc = gpu::TextureDesc::DescCube(1024, 1024, flags, gpu::Format::R32G32B32A32_Float);
		texDesc.m_mipLevels = gfx::MipChainLength(1024, 1024);
		m_cubeMap = gpu::CreateTexture(texDesc, nullptr, "CUBE_TEST");
	}

	{
		gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource;
		gpu::TextureDesc const texDesc = gpu::TextureDesc::DescCube(32, 32, flags, gpu::Format::R16B16G16A16_Float);
		m_scene.m_iblIrradiance = gpu::CreateTexture(texDesc, nullptr, "IBL Irradiance");
	}

	{
		gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource;
		gpu::TextureDesc texDesc = gpu::TextureDesc::DescCube(1024, 1024, flags, gpu::Format::R16B16G16A16_Float);
		texDesc.m_mipLevels = gfx::MipChainLength(1024, 1024);
		m_scene.m_iblGgx = gpu::CreateTexture(texDesc, nullptr, "IBL GGX");
	}

	{
		m_shadowMapPso = gfx::CreateShadowMapPSO(gpu::Format::D32_Float);
	}

	m_skyboxRenderer.Init(m_cubeMap);

	{
		gpu::GraphicsPSODesc psoDesc;
		psoDesc.m_depthFormat = gpu::BackbufferDepthFormat();
		psoDesc.m_numRenderTargets = 1;
		psoDesc.m_renderTargetFormats[0] = gpu::BackbufferFormat();
		psoDesc.m_vertexLayout = gfx::Scene::ManualFetchInstancedVertexLayout();
		psoDesc.m_vs = vertexShader;
		psoDesc.m_ps = pixelShader;

		m_pso = gpu::CreateGraphicsPSO(psoDesc, "Object PSO Test");
		
		m_modelIdx = gfx::ResourceManager::CreateModelFromGLTF("models/rainier_ak/Scene.gltf");
		//m_modelIdx = gfx::ResourceManager::CreateModelFromGLTF("models/sponza/Sponza.gltf");
		m_modelIdx = gfx::ResourceManager::CreateModelFromGLTF("models/DamagedHelmet/DamagedHelmet.gltf");
		m_scene.AddModelInstance(m_modelIdx, kt::Mat4::Identity());
		//m_modelIdx = gfx::ResourceManager::CreateModelFromGLTF("models/MetalRoughSpheres/MetalRoughSpheres.gltf");
	}

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	//gfx::CreateCubemapFromEquirect(ctx, "textures/qwantani_2k.hdr", m_cubeMap);
	//gfx::CreateCubemapFromEquirect(ctx, "textures/Alexs_Apt_2k.hdr", m_cubeMap);
	//gfx::CreateCubemapFromEquirect(ctx, "textures/environment.hdr", m_cubeMap);
	//gfx::CreateCubemapFromEquirect(ctx, "textures/cayley_interior_2k.hdr", m_cubeMap);
	gfx::CreateCubemapFromEquirect(ctx, "textures/syferfontein_1d_clear_2k.hdr", m_cubeMap);

	
	gpu::GenerateMips(ctx, m_cubeMap);

	gfx::BakeEnvMapGGX(ctx, m_cubeMap, m_scene.m_iblGgx);

	{
		gpu::cmd::SetPSO(ctx, gfx::ResourceManager::GetSharedResources().m_bakeIrradPso);
		gpu::cmd::ResourceBarrier(ctx, m_scene.m_iblIrradiance, gpu::ResourceState::UnorderedAccess);

		gpu::DescriptorData srv;
		srv.Set(m_cubeMap);

		gpu::DescriptorData uav;
		uav.Set(m_scene.m_iblIrradiance);

		gpu::cmd::SetComputeSRVTable(ctx, srv, 0);
		gpu::cmd::SetComputeUAVTable(ctx, uav, 0);

		gpu::cmd::Dispatch(ctx, 32 / 32, 32 / 32, 6);
		gpu::cmd::ResourceBarrier(ctx, m_scene.m_iblIrradiance, gpu::ResourceState::ShaderResource);
	}
}

void ShadowTest(gpu::cmd::Context* _ctx, TestbedApp& _app)
{
	KT_UNUSED2(_ctx, _app);

	gpu::cmd::SetViewportAndScissorRectFromTexture(_ctx, _app.m_scene.m_shadowCascadeTex, 0.0f, 1.0f);
	gpu::cmd::ResourceBarrier(_ctx, _app.m_scene.m_shadowCascadeTex, gpu::ResourceState::DepthStencilTarget);
	gpu::cmd::SetRenderTarget(_ctx, 0, gpu::TextureHandle{});

	// TODO: Hack because barrier handling is bad atm.
	gpu::cmd::FlushBarriers(_ctx);

	for (uint32_t cascadeIdx = 0; cascadeIdx < c_numShadowCascades; ++cascadeIdx)
	{
		gpu::cmd::ClearDepth(_ctx, _app.m_scene.m_shadowCascadeTex, 1.0f, cascadeIdx);
		gpu::cmd::SetDepthBuffer(_ctx, _app.m_scene.m_shadowCascadeTex, cascadeIdx);
		gpu::DescriptorData cbv;
		cbv.Set(_app.m_scene.m_shadowCascades[cascadeIdx].GetViewProj().Data(), sizeof(kt::Mat4));

		gpu::cmd::SetGraphicsCBVTable(_ctx, cbv, PATHOS_PER_VIEW_SPACE);
		gpu::cmd::SetPSO(_ctx, _app.m_shadowMapPso);

		_app.m_scene.RenderInstances(_ctx);
	}

	gpu::cmd::ResourceBarrier(_ctx, _app.m_scene.m_shadowCascadeTex, gpu::ResourceState::ShaderResource);
}

void TestbedApp::Tick(float _dt)
{
	gpu::SetVsyncEnabled(s_vsync);

	uint32_t swapchainW, swapchainH;
	gpu::GetSwapchainDimensions(swapchainW, swapchainH);

	gfx::Camera::ProjectionParams params;
	params.SetPerspective(s_camNearPlane, s_camFarPlane, kt::ToRadians(s_camFov), float(swapchainW) / float(swapchainH));
	m_cam.SetProjection(params);

	m_camController.UpdateCamera(_dt, m_cam);
	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	m_scene.BeginFrameAndUpdateBuffers(ctx, m_cam, _dt);
	m_scene.BindPerFrameConstants(ctx);

	ShadowTest(ctx, *this);

	gpu::TextureHandle backbuffer = gpu::CurrentBackbuffer();
	gpu::TextureHandle depth = gpu::BackbufferDepth();

	gpu::cmd::SetPSO(ctx, m_pso);
	
	gpu::DescriptorData cbvs;
	cbvs.Set(m_scene.m_frameConstantsGpuBuf);

	gpu::cmd::SetGraphicsCBVTable(ctx, cbvs, PATHOS_PER_FRAME_SPACE);

	gpu::cmd::SetRenderTarget(ctx, 0, backbuffer);
	gpu::cmd::SetDepthBuffer(ctx, depth);

	float const col[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	gpu::cmd::ClearRenderTarget(ctx, backbuffer, col);
	gpu::cmd::ClearDepth(ctx, depth, 1.0f);

	gpu::cmd::SetViewportAndScissorRectFromTexture(ctx, backbuffer, 0.0f, 1.0f);

	gpu::cmd::SetGraphicsSRVTable(ctx, gfx::ResourceManager::GetTextureDescriptorTable(), PATHOS_CUSTOM_SPACE);

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
