#pragma once
#include <kt/kt.h>
#include <kt/StaticFunction.h>

namespace input
{

constexpr uint32_t c_maxGamepads = 4u;

// Flags for gamepad buttons for a X360 style controller. 
// Note: Flags map directly to XInput bits.
enum class GamepadButton : uint32_t
{
	DPadUp = 0x1,
	DPadDown = 0x2,
	DPadLeft = 0x4,
	DPadRight = 0x8,

	Start = 0x10,
	Back = 0x20,

	LeftThumb = 0x40,
	RightThumb = 0x80,

	LeftShoulder = 0x100,
	RightShoulder = 0x200,

	A = 0x1000,
	B = 0x2000,
	X = 0x4000,
	Y = 0x8000
};

KT_ENUM_CLASS_FLAG_OPERATORS(GamepadButton);

enum class MouseButton : uint32_t
{
	Left,
	Right,
	Middle,
	Mouse3,
	Mouse4,
	Mouse5,
};


enum class Key : uint8_t
{
	InvalidKey = 0,

	Space,
	Apostraphe,
	Comma,
	Minus,
	Period,
	Slash,

	Semicolon,

	Key0,
	Key1,
	Key2,
	Key3,
	Key4,
	Key5,
	Key6,
	Key7,
	Key8,
	Key9,

	KeyA,
	KeyB,
	KeyC,
	KeyD,
	KeyE,
	KeyF,
	KeyG,
	KeyH,
	KeyI,
	KeyJ,
	KeyK,
	KeyL,
	KeyM,
	KeyN,
	KeyO,
	KeyP,
	KeyQ,
	KeyR,
	KeyS,
	KeyT,
	KeyU,
	KeyV,
	KeyW,
	KeyX,
	KeyY,
	KeyZ,

	LeftBracket,
	BackSlash,
	RightBracket,
	Tilde,
	Escape,
	Enter,
	Tab,
	BackSpace,
	Insert,
	Delete,
	Right,
	Left,
	Down,
	Up,
	PageUp,
	PageDown,
	Home,
	End,
	CapsLock,
	ScrollLock,
	NumLock,
	PrintScreen,
	Pause,

	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	F13,
	F14,
	F15,
	F16,
	F17,
	F18,
	F19,
	F20,
	F21,
	F22,
	F23,
	F24,

	NumPad0,
	NumPad1,
	NumPad2,
	NumPad3,
	NumPad4,
	NumPad5,
	NumPad6,
	NumPad7,
	NumPad8,
	NumPad9,
	NumPadDecimal,
	NumPadDivide,
	NumPadMultiply,
	NumPadSubtract,
	NumPadAdd,

	LeftShift,
	LeftControl,
	LeftAlt,
	RightShift,
	RightControl,
	RightAlt,

	MaxKeys
};


struct GamepadState
{
	// Bitfield of buttons pressed since last input update. 
	// Flags: GamepadButton 
	GamepadButton m_buttonsPressed;

	// Bitfield of buttons released since last input update. 
	// Flags: GamepadButton 
	GamepadButton m_buttonsReleased;

	// Bitfield of buttons currently down as of last input update. 
	// Flags: GamepadButton
	GamepadButton m_buttonsDown;

	// Normalized [0, 1] left analog trigger. 
	float m_leftTrigger;

	// Normalized [0, 1] for right analog trigger. 
	float m_rightTrigger;

	// Normalized [0, 1] for left thumb analog stick.
	// Stored as 2 dimensional vector (x, y). 
	float m_leftThumb[2];

	// Normalized [0, 1] for right thumb analog stick. 
	// Stored as 2 dimensional vector (x, y). 
	float m_rightThumb[2];
};

struct Event
{
	static Event Create_GamepadUp(uint32_t _padIdx, GamepadButton _button);
	static Event Create_GamepadDown(uint32_t _padIdx, GamepadButton _button);
	static Event Create_MouseButtonUp(MouseButton _button);
	static Event Create_MouseButtonDown(MouseButton _button);
	static Event Create_MouseWheelDelta(int16_t _delta);
	static Event Create_KeyUp(Key _key);
	static Event Create_KeyDown(Key _key);

	enum class Type
	{
		GamepadUp,
		GamepadDown,
		
		TextInput,
		
		MouseButtonDown,
		MouseButtonUp,
		MouseWheelDelta,
		
		KeyUp,
		KeyDown
	} m_type;

	union 
	{
		// GamePad event state.
		// Type: GamepadUp/GamepadDown
		struct
		{
			// The button in question.
			GamepadButton m_button;

			// The pad this event originated from.
			uint32_t m_padIdx;
		} m_gamepad;

		// Type: MouseWheelDelta
		int16_t m_wheelDelta;

		// Mouse button state.
		// Type: MouseButtonUp/MouseButtonDown
		MouseButton m_mouseButton;

		// Type: KeyUp/KeyDown
		Key m_key;

		// Text string input (UTF-8)
		// Type: TextInput
		char m_stringUtf8[5];
	};
};

// Event callback for client code.
using EventCallback = kt::StaticFunction<void(input::Event const&), 32>;
}