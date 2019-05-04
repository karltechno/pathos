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


static core::CVar<float> s_testCVar0("app.group1.cats", "Test", 0.0f, 1.0f, 2.0f);
static core::CVar<kt::Vec3> s_testCVar1("app.group1.cats2", "Test", kt::Vec3(0.0f), 1.0f, 2.0f);
static core::CVar<float> s_testCVar2("app.group2.cats2", "Test", 0.0f, 1.0f, 2.0f);

static core::CVar<float> s_testCVar01("gfx.cam.cats23", "This is a description.", 0.0f, 1.0f, 2.0f);
static core::CVar<float> s_testCVar11("gfx.cam.cats2", "Test", 0.0f, 1.0f, 2.0f);
static core::CVar<float> s_testCVar21("gfx.group2.cats2", "Test", 0.0f, 1.0f, 2.0f);
static core::CVar<bool> s_testCVar212("gfx.group2.aBool", "Test", true);

static core::CVar<int32_t> s_intTest("app.group1.int32", "testing int", 0, -100, 100);

static core::CVar<float> s_camFov("app.cam.fov", "Camera field of view", 65.0f, 40.0f, 100.0f);

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

	m_modelHandle = res::LoadResourceSync<gfx::Model>("models/DamagedHelmet/DamagedHelmet.gltf");

}

static void DrawModel(gpu::cmd::Context* _cmd, gfx::Model const& _model)
{
	gpu::cmd::SetVertexBuffer(_cmd, 0, _model.m_posGpuBuf);
	gpu::cmd::SetVertexBuffer(_cmd, 1, _model.m_tangentGpuBuf);
	gpu::cmd::SetVertexBuffer(_cmd, 2, _model.m_uv0GpuBuf);
	gpu::cmd::SetIndexBuffer(_cmd, _model.m_indexGpuBuf);

	for (gfx::Model::SubMesh const& mesh : _model.m_meshes)
	{
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
	params.m_nearPlane = 0.01f;
	params.m_type = gfx::Camera::ProjType::Perspective;
	params.m_viewHeight = float(swapchainH);
	params.m_viewWidth = float(swapchainW);

	m_cam.SetProjection(params);

	input::GamepadState gpState;
	if (input::GetGamepadState(0, gpState))
	{
		m_camController.HandleGamepadAnalog(gpState);
	}

	m_camController.UpdateCamera(_dt, m_cam);
	m_myCbuffer.myVec4 += kt::Vec4(_dt);
	m_myCbuffer.mvp = m_cam.GetCachedViewProj();

	gpu::cmd::Context* ctx = gpu::cmd::Begin(gpu::cmd::ContextType::Graphics);

	gpu::TextureHandle backbuffer = gpu::CurrentBackbuffer();
	gpu::TextureHandle depth = gpu::BackbufferDepth();

	gpu::cmd::SetGraphicsPSO(ctx, m_pso);
	gpu::cmd::UpdateTransientBuffer(ctx, m_constantBuffer, &m_myCbuffer);

	gpu::cmd::SetConstantBuffer(ctx, m_constantBuffer, 0, 0);

	float const col[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	gpu::cmd::ClearRenderTarget(ctx, backbuffer, col);
	gpu::cmd::ClearDepth(ctx, depth, 1.0f);
	gpu::cmd::SetRenderTarget(ctx, 0, backbuffer);
	gpu::cmd::SetDepthBuffer(ctx, depth);

	
	{
		// draw test model
		//gfx::Model* model = res::GetData(m_modelHandle);
		//gpu::cmd::SetIndexBuffer(ctx, model->m_indexGpuBuf);
		//gpu::cmd::SetVertexBuffer(ctx, 0, model->m_posGpuBuf);
		//gpu::cmd::DrawIndexedInstanced(ctx, gpu::PrimitiveType::TriangleList, model->m_indicies.Size(), 1, 0, 0, 0);
		DrawModel(ctx, *res::GetData(m_modelHandle));
	}

	gpu::cmd::End(ctx);

}

void TestbedApp::Shutdown()
{
	gpu::Release(m_pso);
	gpu::Release(m_constantBuffer);
}

void TestbedApp::HandleInputEvent(input::Event const& _ev)
{
	m_camController.DefaultInputHandler(_ev);
}

PATHOS_APP_IMPLEMENT_MAIN(TestbedApp);
