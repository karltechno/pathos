#include "App.h"

#include <gpu/GPUDevice.h>
#include <input/Input.h>

#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>

//static kt::Vec3 const s_testTriVerts[] =
//{
//	{ 0.0f, 1.0f, 0.0f },
//	{ 1.0f, -1.0f, 0.0f },
//	{ -1.0f, -1.0f, 0.0f },
//};
//
//static uint16_t const s_testIndicies[] =
//{
//	0, 1, 2
//};

namespace app
{

GraphicsApp::GraphicsApp()
{
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

	kt::TimePoint lastFrameStart = kt::TimePoint::Now();
	kt::Duration tickTime = kt::Duration::FromMilliseconds(16.0);

	do 
	{
		gpu::BeginFrame();

		float const dt = float(tickTime.Milliseconds());
		PumpMessageLoop(m_window);
		input::Tick(dt);
		Tick(dt);


		{
			gpu::CommandContext ctx = gpu::CreateGraphicsContext();

			gpu::TextureHandle backbuffer = gpu::CurrentBackbuffer();

			float col[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
			ctx.ClearRenderTarget(backbuffer, col);
			ctx.End();
		}


		kt::TimePoint const now = kt::TimePoint::Now();
		tickTime = now - lastFrameStart;
		lastFrameStart = now;

		gpu::EndFrame();
	} while (m_keepAlive);

	Shutdown();

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