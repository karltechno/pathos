#pragma once
#include <kt/Platform.h>
#include <stdint.h>

namespace app
{

struct GraphicsApp;

struct WindowHandle
{
	void* nwh;
};

struct WindowInitParams
{
	char const* m_name = nullptr;
	
	uint32_t m_width = 0;
	uint32_t m_height = 0;

	bool m_windowed = false;
};

WindowHandle CreatePlatformWindow(WindowInitParams const& _params);
void SetWindowApp(WindowHandle _wh, GraphicsApp* _app);

void PumpMessageLoop(WindowHandle _hndl);

}