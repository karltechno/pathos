#include "InputTypes.h"

namespace input
{

Event Event::Create_GamepadUp(uint32_t _padIdx, GamepadButton _button)
{
	Event e = {};
	e.m_type = Event::Type::GamepadUp;
	e.m_gamepad.m_button = _button;
	e.m_gamepad.m_padIdx = _padIdx;
	return e;
}

Event Event::Create_GamepadDown(uint32_t _padIdx, GamepadButton _button)
{
	Event e = {};
	e.m_type = Event::Type::GamepadDown;
	e.m_gamepad.m_button = _button;
	e.m_gamepad.m_padIdx = _padIdx;
	return e;
}

}