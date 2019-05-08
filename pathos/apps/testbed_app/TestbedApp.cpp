#include "TestbedApp.h"

#include <stdio.h>

#include <core/CVar.h>
#include <editor/Editor.h>
#include <gfx/Model.h>
#include <gpu/GPUDevice.h>
#include <input/Input.h>


#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include "imgui.h"

static core::CVar<float> s_camFov("cam.fov", "Camera field of view", 65.0f, 40.0f, 100.0f);

enum class MyEnumTest
{
	Hello,
	Cats,
	Test,

	Num
};

char const* const s_enumStrs[] =
{
	"Hello",
	"Cats",
	"Test"
};

static core::CVarEnum<MyEnumTest, MyEnumTest::Num> s_enumCvar("app.enumtest", "test", s_enumStrs, MyEnumTest::Hello);


void TestbedApp::Setup()
{
	m_pixelShader = res::LoadResourceSync<gfx::ShaderResource>("shaders/ObjectShader.ps.cso");
	m_vertexShader = res::LoadResourceSync<gfx::ShaderResource>("shaders/ObjectShader.vs.cso");

	res::ResourceHandle<gfx::ShaderResource> csShader = res::LoadResourceSync<gfx::ShaderResource>("shaders/ClearTest.cs.cso");

	m_csPso = gpu::CreateComputePSO(res::GetData(csShader)->m_shader);

	gpu::GraphicsPSODesc psoDesc;
	psoDesc.m_depthFormat = gpu::Format::D32_Float;
	psoDesc.m_numRenderTargets = 1;
	psoDesc.m_renderTargetFormats[0] = gpu::Format::R8G8B8A8_UNorm;
	psoDesc.m_vertexLayout = gfx::Model::FullVertexLayout();
	psoDesc.m_vs = res::GetData(m_vertexShader)->m_shader;
	psoDesc.m_ps = res::GetData(m_pixelShader)->m_shader;

	m_pso = gpu::CreateGraphicsPSO(psoDesc);

	m_myCbuffer.myVec4 = kt::Vec4(0.0f);

	gpu::BufferDesc constantBufferDesc;
	constantBufferDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Transient;
	constantBufferDesc.m_sizeInBytes = sizeof(DummyCbuffer);
	m_constantBuffer = gpu::CreateBuffer(constantBufferDesc);

	//m_modelHandle = res::LoadResourceSync<gfx::Model>("models/DamagedHelmet/DamagedHelmet.gltf");
	m_modelHandle = res::LoadResourceSync<gfx::Model>("models/sponza/Sponza.gltf");


	gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::UnorderedAccess | gpu::TextureUsageFlags::RenderTarget | gpu::TextureUsageFlags::ShaderResource;
	gpu::TextureDesc const texDesc = gpu::TextureDesc::Desc2D(1280, 720, flags, gpu::Format::R8G8B8A8_UNorm);
	m_testTexture = gpu::CreateTexture(texDesc);
}


static void DrawModel(gpu::cmd::Context* _cmd, gfx::Model const& _model)
{
	gpu::cmd::SetVertexBuffer(_cmd, 0, _model.m_posGpuBuf);
	gpu::cmd::SetVertexBuffer(_cmd, 1, _model.m_tangentGpuBuf);
	gpu::cmd::SetVertexBuffer(_cmd, 2, _model.m_uv0GpuBuf);
	gpu::cmd::SetIndexBuffer(_cmd, _model.m_indexGpuBuf);

	for (gfx::Model::SubMesh const& mesh : _model.m_meshes)
	{

		gfx::Material const& mat = _model.m_materials[mesh.m_materialIdx];

		if (mat.m_diffuseTex.IsValid())
		{
			gpu::cmd::SetSRV(_cmd, res::GetData(mat.m_diffuseTex)->m_gpuTex, 0, 0);
		}

		if (mat.m_normalTex.IsValid())
		{
			gpu::cmd::SetSRV(_cmd, res::GetData(mat.m_normalTex)->m_gpuTex, 1, 0);
		}

		if (mat.m_normalTex.IsValid())
		{
			gpu::cmd::SetSRV(_cmd, res::GetData(mat.m_metallicRoughnessTex)->m_gpuTex, 2, 0);
		}

		if (mat.m_occlusionTex.IsValid())
		{
			gpu::cmd::SetSRV(_cmd, res::GetData(mat.m_occlusionTex)->m_gpuTex, 3, 0);
		}

		gpu::cmd::DrawIndexedInstanced(_cmd, gpu::PrimitiveType::TriangleList, mesh.m_numIndicies, 1, mesh.m_indexBufferStartOffset, 0, 0);
	}
}


void TestbedApp::Tick(float _dt)
{
	uint32_t swapchainW, swapchainH;
	gpu::GetSwapchainDimensions(swapchainW, swapchainH);

	gfx::Camera::ProjectionParams params;
	params.m_farPlane = 10000.0f;
	params.m_fov = kt::ToRadians(s_camFov);
	params.m_nearPlane = 10.0f;
	params.m_type = gfx::Camera::ProjType::Perspective;
	params.m_viewHeight = float(swapchainH);
	params.m_viewWidth = float(swapchainW);

	m_cam.SetProjection(params);

	m_camController.UpdateCamera(_dt, m_cam);
	m_myCbuffer.myVec4 += kt::Vec4(_dt);
	m_myCbuffer.mvp = m_cam.GetCachedViewProj();

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();

	gpu::TextureHandle backbuffer = gpu::CurrentBackbuffer();
	gpu::TextureHandle depth = gpu::BackbufferDepth();

	gpu::cmd::SetPSO(ctx, m_pso);
	gpu::cmd::UpdateTransientBuffer(ctx, m_constantBuffer, &m_myCbuffer, sizeof(m_myCbuffer));

	gpu::cmd::SetCBV(ctx, m_constantBuffer, 0, 0);

	gpu::cmd::SetRenderTarget(ctx, 0, backbuffer);
	gpu::cmd::SetDepthBuffer(ctx, depth);

#if 11
	// HACK FAKE WORK
	for (uint32_t i = 0; i < 1; ++i)
	{
		float const col[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		gpu::cmd::ClearRenderTarget(ctx, backbuffer, col);
		gpu::cmd::ClearDepth(ctx, depth, 1.0f);
	}
#endif


	DrawModel(ctx, *res::GetData(m_modelHandle));
}

void TestbedApp::Shutdown()
{
	gpu::Release(m_testTexture);
	gpu::Release(m_csPso);
	gpu::Release(m_pso);
	gpu::Release(m_constantBuffer);
}

void TestbedApp::HandleInputEvent(input::Event const& _ev)
{
	m_camController.DefaultInputHandler(_ev);
}

PATHOS_APP_IMPLEMENT_MAIN(TestbedApp);
