#pragma once
#include "Window.h"
#include <editor/Windows/GPUWindows.h>

namespace input
{
struct Event;
}

namespace app
{

struct GraphicsApp
{
	GraphicsApp();
	virtual ~GraphicsApp() {}
	
	void Go(WindowHandle _wh, int _argc, char** _argv);
	void RequestShutdown();

	virtual void Setup() = 0;
	virtual void Tick(float _dt) = 0;
	virtual void Shutdown() = 0;

	virtual void HandleInputEvent(input::Event const& _event) = 0;

	void* NativeWindowHandle() const;

private:
	void SubsystemPreable(int _argc, char** _argv);
	void SubsystemPostable();

	editor::GPUWindows m_gpuDebugWindow;

	WindowHandle m_window;
	bool m_keepAlive = true;
};

}

#define PATHOS_APP_IMPLEMENT_MAIN(MY_APP_TYPE) \
int main(int _argc, char** _argv) \
{ \
	app::WindowHandle PATHOS_INIT(int _argc, char** _argv);\
	void PATHOS_SHUTDOWN();\
	app::WindowHandle const wh = PATHOS_INIT(_argc, _argv);\
	{\
		MY_APP_TYPE myApp; \
		myApp.Go(wh, _argc, _argv); \
	}\
	PATHOS_SHUTDOWN();\
}