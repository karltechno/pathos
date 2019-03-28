#pragma once
#include <Input/Input.h>

namespace input
{

namespace win
{

bool Init(void* _nativeWindowHandle, EventCallback _callback, void* _eventUserData);

void Shutdown();

bool IsGamepadConnected(uint32_t _padIdx);

bool GetGamepadState(uint32_t _padIdx, GamepadState& o_state);

void Tick(float const _dt);


// The application must call this within their Windows message loop.
// A return value of 1 indicates the input system processed the message and the application should discard it.
// Otherwise the application should process it (or pass to DefWindowProc).
uint32_t WinMsgLoopHook(void* _hwnd, uint32_t _umsg, uintptr_t _wparam, intptr_t _lparam);

}

}