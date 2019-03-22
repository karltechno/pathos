#include "App.h"

#include "input/Input.h"

#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>

namespace app
{

WindowedApp::WindowedApp()
{
}

void WindowedApp::Go(int _argc, char** _argv)
{
	KT_UNUSED2(_argc, _argv);
	
	WindowInitParams params{};
	params.m_app = this;
	params.m_height = 720;
	params.m_width = 1280;
	params.m_name = "Pathos";
	params.m_windowed = true;

	m_window = CreatePlatformWindow(params);

	if (!input::Init(m_window.nwh, [](void* _ctx, input::Event const& _ev) { ((WindowedApp*)_ctx)->HandleInputEvent(_ev); }, this))
	{
		KT_ASSERT(false);
		KT_LOG_ERROR("Failed to initialise input system, exiting.");
		return;
	}

	Setup();

	kt::TimePoint lastFrameStart = kt::TimePoint::Now();
	kt::Duration tickTime = kt::Duration::FromMilliseconds(16.0);

	do 
	{
		float const dt = float(tickTime.Milliseconds());
		PumpMessageLoop(m_window);
		input::Tick(dt);
		Tick(dt);
		kt::TimePoint const now = kt::TimePoint::Now();
		tickTime = now - lastFrameStart;
		lastFrameStart = now;
	} while (m_keepAlive);

	Shutdown();

	input::Shutdown();
}

void WindowedApp::RequestShutdown()
{
	m_keepAlive = false;
}

void* WindowedApp::NativeWindowHandle() const
{
	return m_window.nwh;
}

}