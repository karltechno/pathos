#pragma once
#include "Window.h"

namespace input
{
struct Event;
}

namespace app
{

struct WindowedApp
{
	WindowedApp();
	virtual ~WindowedApp() {}
	
	void Go(int _argc, char** _argv);
	void RequestShutdown();

	virtual void Setup() = 0;
	virtual void Tick(float const _dt) = 0;
	virtual void Shutdown() = 0;

	// Todo: Input params.
	virtual void HandleInputEvent(input::Event const& _event) = 0;

	void* NativeWindowHandle() const;

private:
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