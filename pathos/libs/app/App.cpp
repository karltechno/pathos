#include "App.h"

#include <stdio.h>

#include <core/CVar.h>
#include <editor/Editor.h>
#include <gpu/GPUDevice.h>
#include <gfx/Resources.h>
#include <input/Input.h>
#include <res/ResourceSystem.h>

#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include "kt/FilePath.h"


namespace app
{

void GraphicsApp::SubsystemPreable(int _argc, char** _argv)
{
	KT_UNUSED2(_argc, _argv);

	WindowInitParams params{};
	params.m_app = this;
	params.m_width = 1280;
	params.m_height = 720;
	params.m_name = "Pathos";
	params.m_windowed = true;

	m_window = CreatePlatformWindow(params);

	input::Init(m_window.nwh, [this](input::Event const& _ev)
	{
		if (!m_imguiHandler.HandleInputEvent(_ev))
		{
			HandleInputEvent(_ev);
		}
	});

	gfx::RegisterResourceLoaders();

	gpu::Init(m_window.nwh);
	editor::Init();
	m_imguiHandler.Init(m_window.nwh);
	core::InitCVars();

	res::Init();
}

void GraphicsApp::SubsystemPostable()
{
	res::Shutdown();
	core::ShutdownCVars();
	m_imguiHandler.Shutdown();
	editor::Shutdown();
	gpu::Shutdown();
	input::Shutdown();
}


GraphicsApp::GraphicsApp()
{
}


void GraphicsApp::Go(int _argc, char** _argv)
{
	SubsystemPreable(_argc, _argv);

	// Setup derived.
	Setup();

	kt::TimePoint lastFrameStart = kt::TimePoint::Now();
	kt::Duration tickTime = kt::Duration::FromMilliseconds(16.0);

	do 
	{
		gpu::BeginFrame();

		float const dt = float(tickTime.Seconds());
		res::Tick();

		PumpMessageLoop(m_window);
		input::Tick(dt);
		m_imguiHandler.BeginFrame(dt);

		editor::Update(dt);

		// Update derived
		Tick(dt);
		
		m_imguiHandler.EndFrame();

		kt::TimePoint const now = kt::TimePoint::Now();
		tickTime = now - lastFrameStart;
		lastFrameStart = now;

		gpu::EndFrame();
	} while (m_keepAlive);

	Shutdown();

	SubsystemPostable();
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