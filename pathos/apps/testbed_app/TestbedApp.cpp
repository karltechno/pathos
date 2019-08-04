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
static core::CVar<bool> s_vsync("app.vsync", "Vsync enabled", true);

constexpr uint32_t c_numShadowCascades = 4;

static const float c_shadowMapRes = 2048.0f;

void TestbedApp::Setup()
{
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
		m_irradMap = gpu::CreateTexture(texDesc, nullptr, "IRRAD_TEST");
	}

	{
		gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::ShaderResource;
		gpu::TextureDesc texDesc = gpu::TextureDesc::DescCube(1024, 1024, flags, gpu::Format::R16B16G16A16_Float);
		texDesc.m_mipLevels = gfx::MipChainLength(1024, 1024);
		m_ggxMap = gpu::CreateTexture(texDesc, nullptr, "GGX_MAP");
	}

	{
		m_shadowMapPso = gfx::CreateShadowMapPSO_Instanced(gpu::Format::D32_Float);
	}

	m_skyboxRenderer.Init(m_cubeMap);

	{
		gpu::GraphicsPSODesc psoDesc;
		psoDesc.m_depthFormat = gpu::BackbufferDepthFormat();
		psoDesc.m_numRenderTargets = 1;
		psoDesc.m_renderTargetFormats[0] = gpu::BackbufferFormat();
		psoDesc.m_vertexLayout = gfx::Model::FullVertexLayoutInstanced();
		psoDesc.m_vs = vertexShader;
		psoDesc.m_ps = pixelShader;

		m_pso = gpu::CreateGraphicsPSO(psoDesc, "Object PSO Test");
		
		m_modelIdx = gfx::ResourceManager::CreateModelFromGLTF("models/DamagedHelmet/DamagedHelmet.gltf");
		//m_modelIdx = gfx::ResourceManager::CreateModelFromGLTF("models/sponza/Sponza.gltf");
		m_modelIdx = gfx::ResourceManager::CreateModelFromGLTF("models/rainier_ak/Scene.gltf");
		//m_modelIdx = gfx::ResourceManager::CreateModelFromGLTF("models/MetalRoughSpheres/MetalRoughSpheres.gltf");
	}

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	//gfx::CreateCubemapFromEquirect(ctx, "textures/qwantani_2k.hdr", m_cubeMap);
	gfx::CreateCubemapFromEquirect(ctx, "textures/Alexs_Apt_2k.hdr", m_cubeMap);
	//gfx::CreateCubemapFromEquirect(ctx, "textures/environment.hdr", m_cubeMap);
	//gfx::CreateCubemapFromEquirect(ctx, "textures/cayley_interior_2k.hdr", m_cubeMap);
	
	gpu::GenerateMips(ctx, m_cubeMap);

	gfx::BakeEnvMapGGX(ctx, m_cubeMap, m_ggxMap);

	{
		gpu::cmd::SetPSO(ctx, gfx::ResourceManager::GetSharedResources().m_bakeIrradPso);
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

void ShadowTest(gpu::cmd::Context* _ctx, TestbedApp& _app)
{
	KT_UNUSED2(_ctx, _app);

	// HACK
	gpu::cmd::SetViewport(_ctx, gpu::Rect{ c_shadowMapRes, c_shadowMapRes }, 0.0f, 1.0f);
	gpu::cmd::SetScissorRect(_ctx, gpu::Rect{ c_shadowMapRes, c_shadowMapRes });
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

		gpu::cmd::SetGraphicsCBVTable(_ctx, cbv, PATHOS_PER_BATCH_SPACE);
		gpu::cmd::SetPSO(_ctx, _app.m_shadowMapPso);

		_app.m_scene.RenderInstances(_ctx, true);
	}

	gpu::cmd::ResourceBarrier(_ctx, _app.m_scene.m_shadowCascadeTex, gpu::ResourceState::ShaderResource);
	// MASSIVE HACK
	gpu::cmd::SetViewport(_ctx, gpu::Rect{ 1280.0f, 720.0f }, 0.0f, 1.0f);
	gpu::cmd::SetScissorRect(_ctx, gpu::Rect{ 1280.0f, 720.0f });
}

void TestbedApp::Tick(float _dt)
{
	gpu::SetVsyncEnabled(s_vsync);

	uint32_t swapchainW, swapchainH;
	gpu::GetSwapchainDimensions(swapchainW, swapchainH);

	gfx::Camera::ProjectionParams params;
	params.SetPerspective(5.0f, 5000.0f, kt::ToRadians(s_camFov), float(swapchainW) / float(swapchainH));
	m_cam.SetProjection(params);

	m_camController.UpdateCamera(_dt, m_cam);
	m_scene.BeginFrameAndUpdateBuffers(gpu::GetMainThreadCommandCtx(), m_cam, _dt);

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();
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

	gpu::DescriptorData frameSrvs[6];
	frameSrvs[0].Set(m_irradMap);
	frameSrvs[1].Set(m_ggxMap);
	frameSrvs[2].Set(gfx::ResourceManager::GetSharedResources().m_ggxLut);
	frameSrvs[3].Set(m_scene.m_lightGpuBuf);
	frameSrvs[4].Set(m_scene.m_shadowCascadeTex);
	frameSrvs[5].Set(gfx::ResourceManager::GetMaterialGpuBuffer());

	gpu::cmd::SetGraphicsSRVTable(ctx, gfx::ResourceManager::GetTextureDescriptorTable(), PATHOS_CUSTOM_SPACE);

	gpu::cmd::SetGraphicsSRVTable(ctx, frameSrvs, PATHOS_PER_FRAME_SPACE);
	m_scene.RenderInstances(ctx, false);

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
