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

Event Event::Create_MouseButtonUp(MouseButton _button)
{
	Event e = {};
	e.m_type = Event::Type::MouseButtonUp;
	e.m_mouseButton = _button;
	return e;
}

Event Event::Create_MouseButtonDown(MouseButton _button)
{
	Event e = {};
	e.m_type = Event::Type::MouseButtonDown;
	e.m_mouseButton = _button;
	return e;
}

input::Event Event::Create_MouseWheelDelta(int16_t _delta)
{
	Event e = {};
	e.m_type = Event::Type::MouseWheelDelta;
	e.m_wheelDelta = _delta;
	return e;
}

input::Event Event::Create_KeyUp(Key _key)
{
	Event e = {};
	e.m_type = Event::Type::KeyUp;
	e.m_key = _key;
	return e;
}

input::Event Event::Create_KeyDown(Key _key)
{
	Event e = {};
	e.m_type = Event::Type::KeyDown;
	e.m_key = _key;
	return e;
}


}