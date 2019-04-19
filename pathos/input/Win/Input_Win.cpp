#include "Input_Win.h"
#include "InputTypes.h"

#include <math.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Xinput.h>

namespace input
{

namespace win
{

using XInputGetStateFn =  DWORD (WINAPI*)(DWORD, XINPUT_STATE*);


struct XInputGamepad
{
	input::GamepadState m_state;
	uint32_t m_lastPacket = UINT32_MAX;
	uint32_t m_isConnected = 0;
};

struct Context
{
	XInputGamepad m_pads[input::c_maxGamepads];
	XInputGetStateFn m_getStateFn;
	HMODULE m_xinputDll = 0;

	input::EventCallback m_eventCb;

	bool m_isInit = false;
};

static Context s_ctx;

static void DispatchInputEvent(input::Event const& _ev)
{
	if (s_ctx.m_eventCb)
	{
		s_ctx.m_eventCb(_ev);
	}
}

BYTE constexpr c_triggerThresh = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
SHORT constexpr c_thumbThresh = 8000;

static float XInputTriggerToFloat(BYTE _trig)
{
	BYTE const clamped = _trig < c_triggerThresh ? 0 : _trig - c_triggerThresh;
	return clamped / (float)(255 - c_triggerThresh);
}


static void XInputThumbToFloat(SHORT i_x, SHORT i_y, float *o_xy)
{
	float const fX = (float)i_x;
	float const fY = (float)i_y;

	float mag = sqrtf(fX * fX + fY * fY);

	float const invMag = 1.0f / mag;

	float const fX_norm = fX * invMag;
	float const fY_norm = fY * invMag;

	float scaledMag = 0.0f;

	if (mag > (float)c_thumbThresh)
	{
		if (mag > (float)INT16_MAX)
		{
			mag = (float)INT16_MAX;
		}

		mag -= c_thumbThresh;

		scaledMag = mag / (float)(INT16_MAX - c_thumbThresh);
	}

	o_xy[0] = fX_norm * scaledMag;
	o_xy[1] = fY_norm * scaledMag;
}

static void XInputUpdateGamepad(XInputGamepad& _gmp, uint32_t _gamepadIdx, XINPUT_STATE const& _state)
{
	// Update buttons. 
	GamepadButton const new_buttons_down = GamepadButton(_state.Gamepad.wButtons);
	GamepadButton const buttons_changed = new_buttons_down ^ _gmp.m_state.m_buttonsDown;
	_gmp.m_state.m_buttonsPressed = buttons_changed & new_buttons_down;
	_gmp.m_state.m_buttonsReleased = buttons_changed & _gmp.m_state.m_buttonsDown;
	_gmp.m_state.m_buttonsDown = new_buttons_down;

	GamepadButton temp_buttons_released = _gmp.m_state.m_buttonsReleased;

	while (uint32_t(temp_buttons_released))
	{
		GamepadButton const bit = temp_buttons_released & ~GamepadButton(uint32_t(temp_buttons_released) - 1);
		temp_buttons_released ^= bit;
		DispatchInputEvent(input::Event::Create_GamepadUp(_gamepadIdx, bit));
	}

	GamepadButton temp_buttons_pressed = _gmp.m_state.m_buttonsPressed;

	while (uint32_t(temp_buttons_pressed))
	{
		GamepadButton const bit = temp_buttons_pressed & ~GamepadButton(uint32_t(temp_buttons_pressed) - 1);
		temp_buttons_pressed ^= bit;
		DispatchInputEvent(input::Event::Create_GamepadDown(_gamepadIdx, bit));
	}

	// Triggers and thumbs. 
	_gmp.m_state.m_leftTrigger = XInputTriggerToFloat(_state.Gamepad.bLeftTrigger);
	_gmp.m_state.m_rightTrigger = XInputTriggerToFloat(_state.Gamepad.bRightTrigger);

	XInputThumbToFloat(_state.Gamepad.sThumbLX, _state.Gamepad.sThumbLY, _gmp.m_state.m_leftThumb);
	XInputThumbToFloat(_state.Gamepad.sThumbRX, _state.Gamepad.sThumbRY, _gmp.m_state.m_rightThumb);
}

static void UpdateXInput(bool _forceRefresh)
{
	for (uint32_t i = 0; i < input::c_maxGamepads; ++i)
	{
		XInputGamepad& xinputGmp = s_ctx.m_pads[i];
		XINPUT_STATE xinp_state;
		if (s_ctx.m_getStateFn(i, &xinp_state) != ERROR_SUCCESS)
		{
			xinputGmp.m_isConnected = 0;
			continue;
		}

		xinputGmp.m_isConnected = 1;

		if (_forceRefresh || xinp_state.dwPacketNumber != xinputGmp.m_lastPacket)
		{
			xinp_state.dwPacketNumber = xinp_state.dwPacketNumber;

			XInputUpdateGamepad(xinputGmp, i, xinp_state);
		}
	}
}

bool Init(void* _nativeWindowHandle, input::EventCallback const& _callback)
{
	KT_UNUSED(_nativeWindowHandle);

	KT_ASSERT(!s_ctx.m_isInit);

	// Init Xinput.

	// https://docs.microsoft.com/en-us/windows/desktop/xinput/xinput-versions
	s_ctx.m_xinputDll = ::LoadLibraryA("XInput1_4.dll");

	if (!s_ctx.m_xinputDll)
	{
		s_ctx.m_xinputDll = ::LoadLibraryA("XInput1_3.dll");
	}

	if (!s_ctx.m_xinputDll)
	{
		s_ctx.m_xinputDll = ::LoadLibraryA("XInput9_1_0.dll");
	}

	if (!s_ctx.m_xinputDll)
	{
		KT_ASSERT(!"Could not load XInput dll.");
		return false;
	}

	s_ctx.m_getStateFn = (XInputGetStateFn)::GetProcAddress(s_ctx.m_xinputDll, "XInputGetState");

	if (!s_ctx.m_getStateFn)
	{
		KT_ASSERT(!"GetProcAddress for XInputGetState failed.");
		::FreeLibrary(s_ctx.m_xinputDll);
		s_ctx.m_xinputDll = 0;
		return false;
	}

	s_ctx.m_eventCb.Clear();

	UpdateXInput(true);

	s_ctx.m_eventCb = _callback;

	s_ctx.m_isInit = true;
	return true;
}



void Shutdown()
{
	KT_ASSERT(s_ctx.m_isInit);
	if (s_ctx.m_xinputDll)
	{
		::FreeLibrary(s_ctx.m_xinputDll);
		s_ctx.m_xinputDll = 0;
	}

	s_ctx.m_getStateFn = nullptr;
	s_ctx.m_isInit = false;
}



bool IsGamepadConnected(uint32_t _padIdx)
{
	KT_ASSERT(s_ctx.m_isInit);
	return _padIdx < input::c_maxGamepads && s_ctx.m_pads[_padIdx].m_isConnected;
}

bool GetGamepadState(uint32_t _padIdx, GamepadState& o_state)
{
	KT_ASSERT(s_ctx.m_isInit);

	if (IsGamepadConnected(_padIdx))
	{
		o_state = s_ctx.m_pads[_padIdx].m_state;
		return true;
	}

	return false;
}

void Tick(float const _dt)
{
	KT_UNUSED(_dt);
	UpdateXInput(false);
}


static uint32_t UTF32_to_UTF8(uint32_t _codepoint, char (&buff)[5])
{
	if (_codepoint <= 0x7F)
	{
		buff[0] = (char)_codepoint;
		buff[1] = 0;
	}
	else if (_codepoint <= 0x7FF)
	{
		buff[0] = 0xC0 | (char)((_codepoint >> 6) & 0x1F);
		buff[1] = 0x80 | (char)(_codepoint & 0x3F);
		buff[2] = 0;
	}
	else if (_codepoint <= 0xFFFF)
	{
		buff[0] = 0xE0 | (char)((_codepoint >> 12) & 0x0F);
		buff[1] = 0x80 | (char)((_codepoint >> 6) & 0x3F);
		buff[2] = 0x80 | (char)(_codepoint & 0x3F);
		buff[3] = 0;
	}
	else if (_codepoint <= 0x10FFFF)
	{
		buff[0] = 0xF0 | (char)((_codepoint >> 18) & 0x0F);
		buff[1] = 0x80 | (char)((_codepoint >> 12) & 0x3F);
		buff[2] = 0x80 | (char)((_codepoint >> 6) & 0x3F);
		buff[3] = 0x80 | (char)(_codepoint & 0x3F);
		buff[4] = 0;
	}
	else
	{
		return 0;
	}

	return 1;
}


uint32_t WinMsgLoopHook(void* _hwnd, uint32_t _umsg, uintptr_t _wparam, intptr_t _lparam)
{
	KT_UNUSED2(_hwnd, _lparam);

	switch (_umsg)
	{
		case WM_UNICHAR:
			if (_wparam == UNICODE_NOCHAR)
			{
				return 0;
			}
		case WM_CHAR:
		{
			input::Event ev = {};
			ev.m_type = input::Event::Type::TextInput;
			KT_ASSERT(_wparam <= UINT32_MAX);
			if (UTF32_to_UTF8(uint32_t(_wparam), ev.m_stringUtf8))
			{
				DispatchInputEvent(ev);
			}
			return 1;
		} break;

		case WM_INPUT:
		{
			//UINT size;

			//::GetRawInputData((HRAWINPUT)_lparam, RID_INPUT, NULL, &size,
			//				sizeof(RAWINPUTHEADER));
			//uint8_t* buff = (uint8_t*)alloca(size);

			//::GetRawInputData((HRAWINPUT)_lparam, RID_INPUT, buff, &size, sizeof(RAWINPUTHEADER));

			//RAWINPUT* raw = (RAWINPUT*)buff;

			//if (raw->header.dwType == RIM_TYPEKEYBOARD)
			//{
			//	RAWKEYBOARD* kb = &raw->data.keyboard;
			//}
			//else if (raw->header.dwType == RIM_TYPEMOUSE)
			//{
			//	RAWMOUSE* mouse = &raw->data.mouse;
			//}
		} break;

		default:
		{
			return 0;
		} break;
	}
	return 0;
}

}

}

