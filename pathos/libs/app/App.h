#pragma once
#include "Window.h"

#include <editor/GPUWindows.h>

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
	
	void Go(int _argc, char** _argv);
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
	MY_APP_TYPE myApp; \
	myApp.Go(_argc, _argv); \
}