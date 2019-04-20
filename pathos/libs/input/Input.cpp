#include "Input.h"
#include "InputTypes.h"

#if KT_PLATFORM_WINDOWS
#include "Win/Input_Win.h"
#endif

namespace input
{



bool Init(void* _nativeWindowHandle, EventCallback const& _callback)
{
#if KT_PLATFORM_WINDOWS
	return win::Init(_nativeWindowHandle, _callback);
#endif
}

void Shutdown()
{
#if KT_PLATFORM_WINDOWS
	win::Shutdown();
#endif
}

void Tick(float const _dt)
{
#if KT_PLATFORM_WINDOWS
	win::Tick(_dt);
#endif
}

bool IsGamepadConnected(uint32_t _padIdx)
{
#if KT_PLATFORM_WINDOWS
	return win::IsGamepadConnected(_padIdx);
#endif
}

bool GetGamepadState(uint32_t _padIdx, GamepadState& o_state)
{
#if KT_PLATFORM_WINDOWS
	return win::GetGamepadState(_padIdx, o_state);
#endif
}


void GetCursorPos(int32_t& o_x, int32_t& o_y)
{
#if KT_PLATFORM_WINDOWS
	return win::GetCursorPos(o_x, o_y);
#endif
}

}