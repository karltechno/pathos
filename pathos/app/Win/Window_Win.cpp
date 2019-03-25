#include <App/Window.h>
#include <input/Win/Input_Win.h>
#include <App/App.h>

#include <kt/Macros.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace app
{

static LRESULT CALLBACK App_WindowProc(HWND _hwnd, UINT _msg, WPARAM _wparam, LPARAM _lparam)
{
	if (input::win::WinMsgLoopHook(_hwnd, _msg, _wparam, _lparam))
	{
		return 0;
	}

	switch (_msg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCTA* create = (CREATESTRUCTA*)_lparam;
			::SetWindowLongPtrA(_hwnd, GWLP_USERDATA, (LPARAM)create->lpCreateParams);
			return TRUE;
		} break;

		case WM_CLOSE:
		{
			app::GraphicsApp* app = (app::GraphicsApp*)::GetWindowLongPtrA(_hwnd, GWLP_USERDATA);
			app->RequestShutdown();
		} break;
	}

	return ::DefWindowProc(_hwnd, _msg, _wparam, _lparam);
}

WindowHandle CreatePlatformWindow(WindowInitParams const& _params)
{
	HINSTANCE const hinst = GetModuleHandle(nullptr);

	static ATOM atom = 0;
	if (!atom)
	{
		WNDCLASSEXA wndClass = {};
		wndClass.cbSize = sizeof(WNDCLASSEXA);
		wndClass.style = CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = App_WindowProc;
		wndClass.hInstance = hinst;
		wndClass.hIcon = ::LoadIcon(hinst, nullptr);
		wndClass.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
		wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wndClass.lpszMenuName = nullptr;
		wndClass.lpszClassName = "PATHOS_WINDOW";
		wndClass.hIconSm = ::LoadIcon(hinst, nullptr);
		atom = ::RegisterClassExA(&wndClass);
		KT_ASSERT(atom);
	}

	int const realScreenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	int const realScreenHeight = ::GetSystemMetrics(SM_CYSCREEN);

	RECT windowRect = { 0, 0, LONG(_params.m_width), LONG(_params.m_height) };
	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	int const windowHeight = windowRect.bottom - windowRect.top;
	int const windowWidth = windowRect.right - windowRect.left;

	HWND const hwnd = ::CreateWindowExA
	(
		0,
		"PATHOS_WINDOW",
		_params.m_name,
		WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX),
		0, 0, // Todo: starting pos?
		windowWidth,
		windowHeight,
		nullptr,
		nullptr,
		hinst,
		_params.m_app
	);

	::SetWindowText(hwnd, _params.m_name);
	::ShowWindow(hwnd, SW_SHOW);
	return WindowHandle{ (void*)hwnd };
}

void PumpMessageLoop(WindowHandle _hndl)
{
	KT_UNUSED(_hndl);

	MSG m;
	while (::PeekMessage(&m, nullptr, 0, 0, PM_REMOVE))
	{
		::TranslateMessage(&m);
		::DispatchMessage(&m);
	}
}

}