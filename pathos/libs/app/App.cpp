#include "App.h"

#include <stdio.h>
#include <stdlib.h>

#include <core/CVar.h>
#include <editor/Editor.h>
#include <input/Input.h>
#include <gfx/Resources.h>
#include <gfx/Scene.h>
#include <gfx/SharedResources.h>
#include <gpu/GPUDevice.h>
#include <res/ResourceSystem.h>

#include <kt/Macros.h>
#include <kt/Timer.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include <kt/FilePath.h>
#include <kt/DebugAllocator.h>

#define PATHOS_CHECK_LEAK (0)

#define PATHOS_FAST_SHUTDOWN (1 && !PATHOS_CHECK_LEAK)

#if PATHOS_CHECK_LEAK
static kt::LeakCheckAllocator s_leakCheckAllocator;
#endif

app::WindowHandle PATHOS_INIT(int _argc, char** _argv)
{
	KT_UNUSED2(_argc, _argv);

#if PATHOS_CHECK_LEAK
	s_leakCheckAllocator.SetAllocatorAndClear(kt::GetDefaultAllocator());
	kt::SetDefaultAllocator(&s_leakCheckAllocator);
#endif

	app::WindowInitParams params{};
	params.m_width = 1280;
	params.m_height = 720;
	params.m_name = "Pathos";
	params.m_windowed = true;

	app::WindowHandle const wh = CreatePlatformWindow(params);

	gfx::RegisterResourceLoaders();

	gpu::Init(wh.nwh);
	editor::Init(wh.nwh);
	core::InitCVars();
	res::Init();
	return wh;
}

void PATHOS_SHUTDOWN()
{
#if PATHOS_FAST_SHUTDOWN
	std::quick_exit(0);
#else
	gfx::ShutdownSharedResources();
	res::Shutdown();
	core::ShutdownCVars();
	editor::Shutdown();
	gpu::Shutdown();
#endif
}

namespace app
{

void GraphicsApp::SubsystemPreable(int _argc, char** _argv)
{
	KT_UNUSED2(_argc, _argv);

	input::Init(m_window.nwh, [this](input::Event const& _ev)
	{
		if (!editor::HandleInputEvent(_ev))
		{
			HandleInputEvent(_ev);
		}
	});
}

void GraphicsApp::SubsystemPostable()
{
	input::Shutdown();
	gfx::Scene::Shutdown();
}


GraphicsApp::GraphicsApp()
{
}


void GraphicsApp::Go(WindowHandle _wh, int _argc, char** _argv)
{
	m_window = _wh;
	app::SetWindowApp(m_window, this);

	SubsystemPreable(_argc, _argv);

	m_gpuDebugWindow.Register();

	// Setup derived.
	gpu::BeginFrame();
	gfx::InitSharedResources();
	Setup();
	gpu::EndFrame();

	kt::TimePoint lastFrameStart = kt::TimePoint::Now();
	kt::Duration tickTime = kt::Duration::FromMilliseconds(16.0);

	do 
	{
		gpu::BeginFrame();

		float const dt = float(tickTime.Seconds());
		res::Tick();

		PumpMessageLoop(m_window);
		input::Tick(dt);
		editor::BeginFrame(dt);
		// Update derived
		Tick(dt);

		editor::Draw(dt);
		editor::EndFrame();

		kt::TimePoint const now = kt::TimePoint::Now();
		tickTime = now - lastFrameStart;
		lastFrameStart = now;

		gpu::EndFrame();
	} while (m_keepAlive);

	Shutdown();

	m_gpuDebugWindow.Unregister();

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