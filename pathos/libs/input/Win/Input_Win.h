#pragma once
#include <input/Input.h>

namespace input
{

namespace win
{

bool Init(void* _nativeWindowHandle, EventCallback const& _callback);

void Shutdown();

bool IsGamepadConnected(uint32_t _padIdx);

bool GetGamepadState(uint32_t _padIdx, GamepadState& o_state);

void Tick(float const _dt);

void GetCursorPos(int32_t& o_x, int32_t& o_y);

// The application must call this within their Windows message loop.
// A return value of 1 indicates the input system processed the message and the application should discard it.
// Otherwise the application should process it (or pass to DefWindowProc).
uint32_t WinMsgLoopHook(void* _hwnd, uint32_t _umsg, uintptr_t _wparam, intptr_t _lparam);

}

}