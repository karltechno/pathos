#include "Input_Win.h"
#include "InputTypes.h"


#include <math.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Xinput.h>
#include <Dbt.h>

#include <kt/Logging.h>

namespace input
{

namespace win
{

using XInputGetStateFn =  DWORD (WINAPI*)(DWORD, XINPUT_STATE*);

static Key s_keyMap[0xFF];

void InitKeyMap()
{
	memset(s_keyMap, 0, sizeof(s_keyMap));

	for (uint32_t i = 0; i < 26; ++i)
	{
		s_keyMap[(int)('A') + i] = (Key)((uint32_t)Key::KeyA + i);
	}

	for (uint32_t i = 0; i < 10; ++i)
	{
		s_keyMap[(int)('0') + i] = (Key)((uint32_t)Key::Key0 + i);
	}

	s_keyMap[VK_ESCAPE] = Key::Escape;
	s_keyMap[VK_RETURN] = Key::Enter;
	s_keyMap[VK_SPACE] = Key::Space;
	s_keyMap[VK_OEM_7] = Key::Apostraphe;
	s_keyMap[VK_OEM_COMMA] = Key::Comma;
	s_keyMap[VK_OEM_MINUS] = Key::Minus;
	s_keyMap[VK_OEM_PERIOD] = Key::Period;
	s_keyMap[VK_OEM_2] = Key::Slash;
	s_keyMap[VK_OEM_5] = Key::BackSlash;
	s_keyMap[VK_OEM_1] = Key::Semicolon;
	s_keyMap[VK_OEM_4] = Key::LeftBracket;
	s_keyMap[VK_OEM_6] = Key::RightBracket;
	s_keyMap[VK_OEM_3] = Key::Tilde;
	s_keyMap[VK_ESCAPE] = Key::Escape;
	s_keyMap[VK_TAB] = Key::Tab;
	s_keyMap[VK_BACK] = Key::BackSpace;
	s_keyMap[VK_INSERT] = Key::Insert;
	s_keyMap[VK_DELETE] = Key::Delete;
	s_keyMap[VK_RIGHT] = Key::Right;
	s_keyMap[VK_LEFT] = Key::Left;
	s_keyMap[VK_DOWN] = Key::Down;
	s_keyMap[VK_UP] = Key::Up;
	s_keyMap[VK_PRIOR] = Key::PageUp;
	s_keyMap[VK_NEXT] = Key::PageDown;
	s_keyMap[VK_HOME] = Key::Home;
	s_keyMap[VK_END] = Key::End;
	s_keyMap[VK_CAPITAL] = Key::CapsLock;
	s_keyMap[VK_SCROLL] = Key::ScrollLock;
	s_keyMap[VK_NUMLOCK] = Key::NumLock;
	s_keyMap[VK_PRINT] = Key::PrintScreen;
	s_keyMap[VK_PAUSE] = Key::Pause;

	s_keyMap[VK_F1] = Key::F1;
	s_keyMap[VK_F2] = Key::F2;
	s_keyMap[VK_F3] = Key::F3;
	s_keyMap[VK_F4] = Key::F4;
	s_keyMap[VK_F5] = Key::F5;
	s_keyMap[VK_F6] = Key::F6;
	s_keyMap[VK_F7] = Key::F7;
	s_keyMap[VK_F8] = Key::F8;
	s_keyMap[VK_F9] = Key::F9;
	s_keyMap[VK_F10] = Key::F10;
	s_keyMap[VK_F11] = Key::F11;
	s_keyMap[VK_F12] = Key::F12;
	s_keyMap[VK_F13] = Key::F13;
	s_keyMap[VK_F14] = Key::F14;
	s_keyMap[VK_F15] = Key::F15;
	s_keyMap[VK_F16] = Key::F16;
	s_keyMap[VK_F17] = Key::F17;
	s_keyMap[VK_F18] = Key::F18;
	s_keyMap[VK_F19] = Key::F19;
	s_keyMap[VK_F20] = Key::F20;
	s_keyMap[VK_F21] = Key::F21;
	s_keyMap[VK_F22] = Key::F22;
	s_keyMap[VK_F23] = Key::F23;
	s_keyMap[VK_F24] = Key::F24;

	s_keyMap[VK_NUMPAD0] = Key::NumPad0;
	s_keyMap[VK_NUMPAD1] = Key::NumPad1;
	s_keyMap[VK_NUMPAD2] = Key::NumPad2;
	s_keyMap[VK_NUMPAD3] = Key::NumPad3;
	s_keyMap[VK_NUMPAD4] = Key::NumPad4;
	s_keyMap[VK_NUMPAD5] = Key::NumPad5;
	s_keyMap[VK_NUMPAD6] = Key::NumPad6;
	s_keyMap[VK_NUMPAD7] = Key::NumPad7;
	s_keyMap[VK_NUMPAD8] = Key::NumPad8;
	s_keyMap[VK_NUMPAD9] = Key::NumPad9;
	s_keyMap[VK_MULTIPLY] = Key::NumPadMultiply;
	s_keyMap[VK_DIVIDE] = Key::NumPadDivide;
	s_keyMap[VK_ADD] = Key::NumPadAdd;
	s_keyMap[VK_DECIMAL] = Key::NumPadDecimal;
	s_keyMap[VK_SUBTRACT] = Key::NumPadSubtract;
	s_keyMap[VK_LSHIFT] = Key::LeftShift;
	s_keyMap[VK_RSHIFT] = Key::RightShift;
	s_keyMap[VK_LCONTROL] = Key::LeftControl;
	s_keyMap[VK_RCONTROL] = Key::RightControl;
	s_keyMap[VK_CONTROL] = Key::LeftControl;
	s_keyMap[VK_LMENU] = Key::LeftAlt;
	s_keyMap[VK_RMENU] = Key::RightAlt;
}

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
	HWND m_hwnd;

	int32_t m_lastCursorPos[2] = {0, 0};

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

static void UpdateConnectedXInputPads()
{
	for (uint32_t i = 0; i < input::c_maxGamepads; ++i)
	{
		XInputGamepad& xinputGmp = s_ctx.m_pads[i];
		XINPUT_STATE xinp_state;
		xinputGmp.m_isConnected = (s_ctx.m_getStateFn(i, &xinp_state) == ERROR_SUCCESS);
	}
}

static void UpdateXInput(bool _forceRefresh)
{
	for (uint32_t i = 0; i < input::c_maxGamepads; ++i)
	{
		XInputGamepad& xinputGmp = s_ctx.m_pads[i];
		if (!xinputGmp.m_isConnected)
		{
			continue;
		}

		XINPUT_STATE xinp_state;
		if (s_ctx.m_getStateFn(i, &xinp_state) != ERROR_SUCCESS)
		{
			xinputGmp.m_isConnected = 0;
			continue;
		}

		if (_forceRefresh || xinp_state.dwPacketNumber != xinputGmp.m_lastPacket)
		{
			xinp_state.dwPacketNumber = xinp_state.dwPacketNumber;

			XInputUpdateGamepad(xinputGmp, i, xinp_state);
		}
	}
}

bool Init(void* _nativeWindowHandle, input::EventCallback const& _callback)
{
	KT_ASSERT(!s_ctx.m_isInit);
	InitKeyMap();

	s_ctx.m_hwnd = HWND(_nativeWindowHandle);

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

	UpdateConnectedXInputPads();
	UpdateXInput(true);

	s_ctx.m_eventCb = _callback;

	RAWINPUTDEVICE rid[2];

	rid[0].usUsagePage = 0x01;
	rid[0].usUsage = 0x02;
	rid[0].dwFlags = 0;
	rid[0].hwndTarget = 0;

	rid[1].usUsagePage = 0x01;
	rid[1].usUsage = 0x06;
	rid[1].dwFlags = 0;
	rid[1].hwndTarget = 0;

	if (::RegisterRawInputDevices(rid, KT_ARRAY_COUNT(rid), sizeof(RAWINPUTDEVICE)) == FALSE)
	{
		KT_LOG_ERROR("RegisterRawInputDevices failed: Error code: %u", ::GetLastError());
		KT_ASSERT(false);
		return false;
	}

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

	POINT p;

	if (::GetCursorPos(&p) && ::ScreenToClient(s_ctx.m_hwnd, &p))
	{
		s_ctx.m_lastCursorPos[0] = p.x;
		s_ctx.m_lastCursorPos[1] = p.y;
	}
}


void GetCursorPos(int32_t& o_x, int32_t& o_y)
{
	o_x = s_ctx.m_lastCursorPos[0];
	o_y = s_ctx.m_lastCursorPos[1];
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
			UINT size;

			::GetRawInputData((HRAWINPUT)_lparam, RID_INPUT, NULL, &size,
							  sizeof(RAWINPUTHEADER));
			uint8_t* buff = (uint8_t*)alloca(size);

			::GetRawInputData((HRAWINPUT)_lparam, RID_INPUT, buff, &size, sizeof(RAWINPUTHEADER));

			RAWINPUT* raw = (RAWINPUT*)buff;

			if (raw->header.dwType == RIM_TYPEKEYBOARD)
			{
				// https://blog.molecular-matters.com/2011/09/05/properly-handling-keyboard-input/
				RAWKEYBOARD* kb = &raw->data.keyboard;
				UINT translatedVK = kb->VKey;

				if (kb->VKey == 255)
				{
					break;
				}
				else if (kb->VKey == VK_SHIFT || kb->VKey == VK_CONTROL) 
				{
					// TODO: Various sources say this is hardware dependant, and need to read e0/e1 to handle lctrl/rctrl properly.
					// Works on my machine atm :)
					translatedVK = ::MapVirtualKeyA(kb->MakeCode, MAPVK_VSC_TO_VK_EX);
				}
				else if (kb->VKey == VK_NUMLOCK)
				{
					translatedVK = ::MapVirtualKey(kb->VKey, MAPVK_VK_TO_VSC);
				}

				if (translatedVK < KT_ARRAY_COUNT(s_keyMap))
				{
					Key const key = s_keyMap[translatedVK];
					if (key == Key::InvalidKey)
					{
						break;
					}

					if (kb->Flags & RI_KEY_BREAK)
					{
						DispatchInputEvent(input::Event::Create_KeyUp(key));
					}
					else
					{
						DispatchInputEvent(input::Event::Create_KeyDown(key));
					}
				}


			}
			else if (raw->header.dwType == RIM_TYPEMOUSE)
			{
				RAWMOUSE* mouse = &raw->data.mouse;
				
				// Handle buttons
				if (mouse->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) { DispatchInputEvent(Event::Create_MouseButtonDown(MouseButton::Left)); }
				if (mouse->usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) { DispatchInputEvent(Event::Create_MouseButtonUp(MouseButton::Left));}
				if (mouse->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) { DispatchInputEvent(Event::Create_MouseButtonDown(MouseButton::Middle)); }
				if (mouse->usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) { DispatchInputEvent(Event::Create_MouseButtonUp(MouseButton::Middle)); }
				if (mouse->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) { DispatchInputEvent(Event::Create_MouseButtonDown(MouseButton::Right)); }
				if (mouse->usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) { DispatchInputEvent(Event::Create_MouseButtonUp(MouseButton::Right)); }
				if (mouse->usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) { DispatchInputEvent(Event::Create_MouseButtonDown(MouseButton::Mouse3)); }
				if (mouse->usButtonFlags & RI_MOUSE_BUTTON_3_UP) { DispatchInputEvent(Event::Create_MouseButtonUp(MouseButton::Mouse3)); }
				if (mouse->usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) { DispatchInputEvent(Event::Create_MouseButtonDown(MouseButton::Mouse4)); }
				if (mouse->usButtonFlags & RI_MOUSE_BUTTON_4_UP) { DispatchInputEvent(Event::Create_MouseButtonUp(MouseButton::Mouse4)); }
				if (mouse->usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) { DispatchInputEvent(Event::Create_MouseButtonDown(MouseButton::Mouse5)); }
				if (mouse->usButtonFlags & RI_MOUSE_BUTTON_5_UP) { DispatchInputEvent(Event::Create_MouseButtonUp(MouseButton::Mouse5)); }

				if (mouse->usButtonFlags & RI_MOUSE_WHEEL)
				{
					DispatchInputEvent(Event::Create_MouseWheelDelta(int16_t(mouse->usButtonData)));
				}
			}
		} break;

		case WM_DEVICECHANGE:
		{
			if ((UINT)_wparam == DBT_DEVNODES_CHANGED)
			{
				UpdateConnectedXInputPads();
			}
			return 0;
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

