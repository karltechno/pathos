#pragma once
#include "InputTypes.h"

namespace input
{
// Global init.
bool Init(void* _nativeWindowHandle, EventCallback const& _callback);

// Global shutdown.
void Shutdown();

// Tick, called once per frame.
void Tick(float const _dt);

// Returns whether the given gamepad is connected.
bool IsGamepadConnected(uint32_t _padIdx);

bool GetGamepadState(uint32_t _padIdx, GamepadState& o_state);

}