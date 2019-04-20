#include "TestbedApp.h"

#include <stdio.h>

#include <core/CVar.h>
#include <editor/Editor.h>
#include <gpu/GPUDevice.h>
#include <input/Input.h>

#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>


static core::CVar<float> s_testCVar0("app.group1.cats", "Test", 0.0f, 1.0f, 2.0f);
static core::CVar<kt::Vec3> s_testCVar1("app.group1.cats2", "Test", kt::Vec3(0.0f), 1.0f, 2.0f);
static core::CVar<float> s_testCVar2("app.group2.cats2", "Test", 0.0f, 1.0f, 2.0f);

static core::CVar<float> s_testCVar01("gfx.cam.cats23", "This is a description.", 0.0f, 1.0f, 2.0f);
static core::CVar<float> s_testCVar11("gfx.cam.cats2", "Test", 0.0f, 1.0f, 2.0f);
static core::CVar<float> s_testCVar21("gfx.group2.cats2", "Test", 0.0f, 1.0f, 2.0f);
static core::CVar<bool> s_testCVar212("gfx.group2.aBool", "Test", true);

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



void TestbedApp::Setup()
{
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

	m_pixelShader = gpu::CreateShader(gpu::ShaderType::Pixel, psCode);
	m_vertexShader = gpu::CreateShader(gpu::ShaderType::Vertex, vsCode);

	gpu::GraphicsPSODesc psoDesc;
	psoDesc.m_depthFormat = gpu::Format::D32_Float;
	psoDesc.m_numRenderTargets = 1;
	psoDesc.m_renderTargetFormats[0] = gpu::Format::R8G8B8A8_UNorm;
	psoDesc.m_vertexLayout.Add(gpu::VertexDeclEntry{ gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Position, 0, 0 });
	psoDesc.m_vs = m_vertexShader;
	psoDesc.m_ps = m_pixelShader;

	gpu::BufferDesc indexBufferDesc;
	indexBufferDesc.m_flags = gpu::BufferFlags::Index;
	indexBufferDesc.m_format = gpu::Format::R16_Uint;
	indexBufferDesc.m_strideInBytes = sizeof(uint16_t);
	indexBufferDesc.m_sizeInBytes = sizeof(s_testIndicies);

	m_indexBuffer = gpu::CreateBuffer(indexBufferDesc, s_testIndicies);

	gpu::BufferDesc vertexBufferDesc;
	vertexBufferDesc.m_flags = gpu::BufferFlags::Vertex;
	vertexBufferDesc.m_format = gpu::Format::Unknown;
	vertexBufferDesc.m_strideInBytes = sizeof(kt::Vec3);
	vertexBufferDesc.m_sizeInBytes = sizeof(s_testTriVerts);
	m_vertexBuffer = gpu::CreateBuffer(vertexBufferDesc, s_testTriVerts);

	m_myCbuffer.myVec4 = kt::Vec4(0.0f);

	gpu::BufferDesc constantBufferDesc;
	constantBufferDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Transient;
	constantBufferDesc.m_sizeInBytes = sizeof(DummyCbuffer);
	m_constantBuffer = gpu::CreateBuffer(constantBufferDesc);

	m_pso = gpu::CreateGraphicsPSO(psoDesc);
}


void TestbedApp::Tick(float _dt)
{
	m_myCbuffer.myVec4 += kt::Vec4(_dt);

	gpu::cmd::Context* ctx = gpu::cmd::Begin(gpu::cmd::ContextType::Graphics);

	gpu::TextureHandle backbuffer = gpu::CurrentBackbuffer();
	gpu::TextureHandle depth = gpu::BackbufferDepth();

	gpu::cmd::SetGraphicsPSO(ctx, m_pso);

	gpu::cmd::SetIndexBuffer(ctx, m_indexBuffer);
	gpu::cmd::SetVertexBuffer(ctx, 0, m_vertexBuffer);
	gpu::cmd::UpdateTransientBuffer(ctx, m_constantBuffer, &m_myCbuffer);

	gpu::cmd::SetConstantBuffer(ctx, m_constantBuffer, 0, 0);
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

void TestbedApp::Shutdown()
{
	gpu::Release(m_indexBuffer);
	gpu::Release(m_vertexBuffer);
	gpu::Release(m_pso);
	gpu::Release(m_pixelShader);
	gpu::Release(m_vertexShader);
	gpu::Release(m_constantBuffer);
}

PATHOS_APP_IMPLEMENT_MAIN(TestbedApp);
