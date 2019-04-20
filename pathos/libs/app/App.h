#pragma once
#include "Window.h"
#include "ImGuiHandler.h"

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
	WindowHandle m_window;
	ImGuiHandler m_imguiHandler;
	bool m_keepAlive = true;
};

}

#define PATHOS_APP_IMPLEMENT_MAIN(MY_APP_TYPE) \
int main(int _argc, char** _argv) \
{ \
	MY_APP_TYPE myApp; \
	myApp.Go(_argc, _argv); \
}