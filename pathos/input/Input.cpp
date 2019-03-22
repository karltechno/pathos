#include "Input.h"

#if KT_PLATFORM_WINDOWS
#include "Win/Input_Win.h"
#endif

namespace input
{



bool Init(void* _nativeWindowHandle, EventCallback _callback, void* _eventUserData)
{
#if KT_PLATFORM_WINDOWS
	return win::Init(_nativeWindowHandle, _callback, _eventUserData);
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


}