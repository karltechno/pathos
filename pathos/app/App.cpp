#include "App.h"

#include <gpu/GPUDevice.h>
#include <input/Input.h>

#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>

#include <stdio.h>

static kt::Vec3 const s_testTriVerts[] =
{
	{ 0.0f, 1.0f, 0.0f },
	{ 1.0f, -1.0f, 0.0f },
	{ -1.0f, -1.0f, 0.0f },
};

static uint16_t const s_testIndicies[] =
{
	0, 1, 2
};

namespace app
{

GraphicsApp::GraphicsApp()
{
}

static void DebugReadEntireFile(FILE* _f, gpu::ShaderBytecode& o_byteCode)
{
	fseek(_f, 0, SEEK_END);
	size_t len = ftell(_f);
	fseek(_f, 0, SEEK_SET);
	void* ptr = kt::Malloc(len);
	fread(ptr, len, 1, _f);
	o_byteCode.m_size = len;
	o_byteCode.m_data = ptr;
}

void GraphicsApp::Go(int _argc, char** _argv)
{
	KT_UNUSED2(_argc, _argv);
	
	WindowInitParams params{};
	params.m_app = this;
	params.m_height = 720;
	params.m_width = 1280;
	params.m_name = "Pathos";
	params.m_windowed = true;

	m_window = CreatePlatformWindow(params);

	if (!input::Init(m_window.nwh, [](void* _ctx, input::Event const& _ev) { ((GraphicsApp*)_ctx)->HandleInputEvent(_ev); }, this))
	{
		KT_ASSERT(false);
		KT_LOG_ERROR("Failed to initialise input system, exiting.");
		return;
	}

	gpu::Init(m_window.nwh);

	Setup();

	FILE* pshFile = fopen("shaders/TestTri.pixel.cso", "rb");
	FILE* vshFile = fopen("shaders/TestTri.vertex.cso", "rb");

	KT_ASSERT(pshFile);
	KT_ASSERT(vshFile);

	KT_SCOPE_EXIT(fclose(pshFile));
	KT_SCOPE_EXIT(fclose(vshFile));

	gpu::ShaderBytecode vsCode, psCode;

	DebugReadEntireFile(pshFile, psCode);
	DebugReadEntireFile(vshFile, vsCode);

	KT_SCOPE_EXIT(kt::Free(psCode.m_data));
	KT_SCOPE_EXIT(kt::Free(vsCode.m_data));

	gpu::ShaderHandle pixelHandle = gpu::CreateShader(gpu::ShaderType::Pixel, psCode);
	gpu::ShaderHandle vertexHandle = gpu::CreateShader(gpu::ShaderType::Vertex, vsCode);

	gpu::GraphicsPSODesc psoDesc;
	psoDesc.m_depthFormat = gpu::Format::D32_Float;
	psoDesc.m_numRenderTargets = 1;
	psoDesc.m_renderTargetFormats[0] = gpu::Format::R8G8B8A8_UNorm;
	psoDesc.m_vertexLayout.Add(gpu::VertexDeclEntry{ gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Position, 0, 0 });
	psoDesc.m_vs = vertexHandle;
	psoDesc.m_ps = pixelHandle;

	gpu::BufferDesc indexBufferDesc;
	indexBufferDesc.m_flags = gpu::BufferFlags::Index;
	indexBufferDesc.m_format = gpu::Format::R16_Uint;
	indexBufferDesc.m_strideInBytes = sizeof(uint16_t);
	indexBufferDesc.m_sizeInBytes = sizeof(s_testIndicies);

	gpu::BufferHandle indexBuffer = gpu::CreateBuffer(indexBufferDesc, s_testIndicies);

	gpu::BufferDesc vertexBufferDesc;
	vertexBufferDesc.m_flags = gpu::BufferFlags::Vertex;
	vertexBufferDesc.m_format = gpu::Format::Unknown;
	vertexBufferDesc.m_strideInBytes = sizeof(kt::Vec3);
	vertexBufferDesc.m_sizeInBytes = sizeof(s_testTriVerts);
	gpu::BufferHandle vertexBuffer = gpu::CreateBuffer(vertexBufferDesc, s_testTriVerts);

	struct KT_ALIGNAS(256) DummyCbuffer
	{
		kt::Vec4 myVec4;
	} myCbuffer;

	myCbuffer.myVec4 = kt::Vec4(0.0f);

	gpu::BufferDesc constantBufferDesc;
	constantBufferDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Transient;
	constantBufferDesc.m_sizeInBytes = sizeof(DummyCbuffer);
	gpu::BufferHandle constantBuffer = gpu::CreateBuffer(constantBufferDesc);


	gpu::GraphicsPSOHandle psoHandle = gpu::CreateGraphicsPSO(psoDesc);
	KT_UNUSED(psoHandle);

	kt::TimePoint lastFrameStart = kt::TimePoint::Now();
	kt::Duration tickTime = kt::Duration::FromMilliseconds(16.0);

	do 
	{
		gpu::BeginFrame();

		float const dt = float(tickTime.Milliseconds());
		PumpMessageLoop(m_window);
		input::Tick(dt);
		Tick(dt);

		myCbuffer.myVec4 += kt::Vec4(dt);
		{
			gpu::cmd::Context* ctx = gpu::cmd::Begin(gpu::cmd::ContextType::Graphics);

			gpu::TextureHandle backbuffer = gpu::CurrentBackbuffer();
			gpu::TextureHandle depth = gpu::BackbufferDepth();

			gpu::cmd::SetGraphicsPSO(ctx, psoHandle);

			gpu::cmd::SetIndexBuffer(ctx, indexBuffer);
			gpu::cmd::SetVertexBuffer(ctx, 0, vertexBuffer);
			gpu::cmd::UpdateTransientBuffer(ctx, constantBuffer, &myCbuffer);

			gpu::cmd::SetConstantBuffer(ctx, constantBuffer, 0, 0);
			uint32_t width, height;
			gpu::GetSwapchainDimensions(width, height);

			gpu::Rect rect{ float(width), float(height) };

			gpu::cmd::SetViewport(ctx, rect, 0.0f, 1.0f);
			gpu::cmd::SetScissorRect(ctx, rect);

			float const col[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
			gpu::cmd::ClearRenderTarget(ctx, backbuffer, col);
			gpu::cmd::ClearDepth(ctx, depth, 1.0f);
			gpu::cmd::SetRenderTarget(ctx, 0, backbuffer);
			gpu::cmd::SetDepthBuffer(ctx, depth);
			gpu::cmd::DrawIndexedInstanced(ctx, gpu::PrimitiveType::TriangleList, 3, 1, 0, 0, 0);
			gpu::cmd::End(ctx);
		}


		kt::TimePoint const now = kt::TimePoint::Now();
		tickTime = now - lastFrameStart;
		lastFrameStart = now;

		gpu::EndFrame();
	} while (m_keepAlive);

	Shutdown();

	gpu::Release(indexBuffer);
	gpu::Release(vertexBuffer);
	gpu::Release(psoHandle);
	gpu::Release(pixelHandle);
	gpu::Release(vertexHandle);
	gpu::Release(constantBuffer);

	gpu::Shutdown();
	input::Shutdown();
}

void GraphicsApp::RequestShutdown()
{
	m_keepAlive = false;
}

void* GraphicsApp::NativeWindowHandle() const
{
	return m_window.nwh;
}

}