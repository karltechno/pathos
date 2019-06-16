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
		res::ResourceHandle<gfx::ShaderResource> skyBoxVS = res::LoadResourceSync<gfx::ShaderResource>("shaders/SkyBox.vs.cso");
		res::ResourceHandle<gfx::ShaderResource> skyBoxPS = res::LoadResourceSync<gfx::ShaderResource>("shaders/SkyBox.ps.cso");
	
		gpu::GraphicsPSODesc skyBoxPso;
		skyBoxPso.m_depthStencilDesc.m_depthEnable = 1;
		skyBoxPso.m_depthStencilDesc.m_depthWrite = 0;
		skyBoxPso.m_depthStencilDesc.m_depthFn = gpu::ComparisonFn::LessEqual;
		skyBoxPso.m_rasterDesc.m_cullMode = gpu::CullMode::Front;
		skyBoxPso.m_depthFormat = gpu::Format::D32_Float;
		skyBoxPso.m_numRenderTargets = 1;
		skyBoxPso.m_renderTargetFormats[0] = gpu::BackbufferFormat();
		skyBoxPso.m_vertexLayout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Position);
		skyBoxPso.m_vs = res::GetData(skyBoxVS)->m_shader;
		skyBoxPso.m_ps = res::GetData(skyBoxPS)->m_shader;
		m_skyBoxPso = gpu::CreateGraphicsPSO(skyBoxPso);
	}

	{
		gpu::GraphicsPSODesc psoDesc;
		psoDesc.m_depthFormat = gpu::Format::D32_Float;
		psoDesc.m_numRenderTargets = 1;
		psoDesc.m_renderTargetFormats[0] = gpu::Format::R8G8B8A8_UNorm;
		psoDesc.m_vertexLayout = gfx::Model::FullVertexLayout();
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


		gpu::BufferDesc lightBufferDesc;
		lightBufferDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Dynamic;
		lightBufferDesc.m_sizeInBytes = sizeof(shaderlib::TestLightCBuffer);
		m_lightCbuffer = gpu::CreateBuffer(lightBufferDesc);

	}
	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	gfx::CreateCubemapFromEquirect("textures/qwantani_2k.hdr", m_cubeMap, ctx);
	gfx::GenerateMips(ctx, m_cubeMap);
	{
		gpu::cmd::SetPSO(ctx, gfx::GetSharedResources().m_bakeIrradPso);
		gpu::cmd::ResourceBarrier(ctx, m_irradMap, gpu::ResourceState::ShaderResource_ReadWrite);

		gpu::DescriptorData srv;
		srv.Set(m_cubeMap);

		gpu::DescriptorData uav;
		uav.Set(m_irradMap);

		gpu::cmd::SetComputeSRVTable(ctx, srv, 0);
		gpu::cmd::SetComputeUAVTable(ctx, uav, 0);

		gpu::cmd::Dispatch(ctx, 32 / 32, 32 / 32, 6);
		gpu::cmd::ResourceBarrier(ctx, m_irradMap, gpu::ResourceState::ShaderResource_Read);
	}


	{
		gfx::PrimitiveBuffers buf;
		buf.m_genFlags = gfx::PrimitiveBuffers::GenFlags::PosOnly;
		gfx::GenCube(buf);
		m_cubeData = gfx::MakePrimitiveGPUBuffers(buf);
	}
}

static gpu::TextureHandle GetTextureHandleOrNull(gfx::TextureResHandle _res)
{
	return _res.IsValid() ? res::GetData(_res)->m_gpuTex : gpu::TextureHandle{};
}

void DrawModel(gpu::cmd::Context* _cmd, gfx::Model const& _model)
{
	gpu::cmd::SetVertexBuffer(_cmd, 0, _model.m_posGpuBuf);
	gpu::cmd::SetVertexBuffer(_cmd, 1, _model.m_tangentGpuBuf);
	gpu::cmd::SetVertexBuffer(_cmd, 2, _model.m_uv0GpuBuf);
	gpu::cmd::SetIndexBuffer(_cmd, _model.m_indexGpuBuf);

	for (gfx::Model::SubMesh const& mesh : _model.m_meshes)
	{
		gfx::Material const& mat = _model.m_materials[mesh.m_materialIdx];
		
		gpu::DescriptorData descriptors[4];
		descriptors[0].Set(GetTextureHandleOrNull(mat.m_albedoTex));
		descriptors[1].Set(GetTextureHandleOrNull(mat.m_normalTex));
		descriptors[2].Set(GetTextureHandleOrNull(mat.m_metallicRoughnessTex));
		descriptors[3].Set(GetTextureHandleOrNull(mat.m_occlusionTex));
		gpu::cmd::SetGraphicsSRVTable(_cmd, descriptors, 0);

		gpu::cmd::DrawIndexedInstanced(_cmd, gpu::PrimitiveType::TriangleList, mesh.m_numIndicies, 1, mesh.m_indexBufferStartOffset, 0, 0);
	}
}

core::CVar<kt::Vec3> s_scale("app.mvp_scale", "test", kt::Vec3(1.0f), -2.0f, 2.0f);


void TestbedApp::Tick(float _dt)
{
	gpu::SetVsyncEnabled(s_vsync);


	m_scene.UpdateCBuffer(&m_testLightCbufferData);
	m_testLightCbufferData.camPos = m_cam.GetPos();

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

	kt::Mat4 scaleMtx = kt::Mat4::Identity();
	scaleMtx[0][0] = s_scale.GetValue()[0];
	scaleMtx[1][1] = s_scale.GetValue()[1];
	scaleMtx[2][2] = s_scale.GetValue()[2];

	m_camController.UpdateCamera(_dt, m_cam);
	m_myCbuffer.mvp = m_cam.GetCachedViewProj() * scaleMtx;

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();
	gfx::GenerateMips(ctx, m_cubeMap);

	gpu::TextureHandle backbuffer = gpu::CurrentBackbuffer();
	gpu::TextureHandle depth = gpu::BackbufferDepth();

	gpu::cmd::ResourceBarrier(ctx, m_lightCbuffer, gpu::ResourceState::CopyDest);
	gpu::cmd::FlushBarriers(ctx);
	gpu::cmd::UpdateDynamicBuffer(ctx, m_lightCbuffer, &m_testLightCbufferData, sizeof(m_testLightCbufferData));
	gpu::cmd::ResourceBarrier(ctx, m_lightCbuffer, gpu::ResourceState::ConstantBuffer);

	gpu::cmd::SetPSO(ctx, m_pso);
	gpu::cmd::UpdateTransientBuffer(ctx, m_constantBuffer, &m_myCbuffer, sizeof(m_myCbuffer));
	
	gpu::DescriptorData cbvs[2];
	cbvs[0].Set(m_constantBuffer);
	cbvs[1].Set(m_lightCbuffer);

	gpu::cmd::SetGraphicsCBVTable(ctx, cbvs, 0);

	gpu::cmd::SetRenderTarget(ctx, 0, backbuffer);
	gpu::cmd::SetDepthBuffer(ctx, depth);


	float const col[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	gpu::cmd::ClearRenderTarget(ctx, backbuffer, col);
	gpu::cmd::ClearDepth(ctx, depth, 1.0f);

	gpu::DescriptorData irradMap;
	irradMap.Set(m_irradMap);

	gpu::cmd::SetGraphicsSRVTable(ctx, irradMap, 1);
	DrawModel(ctx, *res::GetData(m_modelHandle));

	{
		// skybox
		gpu::cmd::SetPSO(ctx, m_skyBoxPso);
		gpu::DescriptorData skyBox;
		skyBox.Set(m_cubeMap);

		gpu::cmd::SetGraphicsSRVTable(ctx, skyBox, 0);
		gpu::cmd::SetVertexBuffer(ctx, 0, m_cubeData.m_pos);
		gpu::cmd::SetIndexBuffer(ctx, m_cubeData.m_indicies);
		kt::Mat4 skyMtx = m_cam.GetView();
		skyMtx.SetPos(kt::Vec3(0.0f));
		skyMtx = m_cam.GetProjection() * skyMtx;

		gpu::DescriptorData skyMtxDescriptor;
		skyMtxDescriptor.Set(&skyMtx, sizeof(skyMtx));

		gpu::cmd::SetGraphicsCBVTable(ctx, skyMtxDescriptor, 0);
		
		uint32_t w, h;
		gpu::GetSwapchainDimensions(w, h);
		gpu::Rect rect{ float(w), float(h) };

		gpu::cmd::SetViewport(ctx, rect, 1.0f, 1.0f);
		gpu::cmd::DrawIndexedInstanced(ctx, gpu::PrimitiveType::TriangleList, m_cubeData.m_numIndicies, 1, 0, 0, 0);
	}
}

void TestbedApp::Shutdown()
{
}

void TestbedApp::HandleInputEvent(input::Event const& _ev)
{
	m_camController.DefaultInputHandler(_ev);
}

PATHOS_APP_IMPLEMENT_MAIN(TestbedApp);
