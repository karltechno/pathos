#include <kt/kt.h>

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
	float left_trigger;

	// Normalized [0, 1] for right analog trigger. 
	float right_trigger;

	// Normalized [0, 1] for left thumb analog stick.
	// Stored as 2 dimensional vector (x, y). 
	float left_thumb[2];

	// Normalized [0, 1] for right thumb analog stick. 
	// Stored as 2 dimensional vector (x, y). 
	float right_thumb[2];
};

struct Event
{
	static Event Create_GamepadUp(uint32_t _padIdx, GamepadButton _button);
	static Event Create_GamepadDown(uint32_t _padIdx, GamepadButton _button);

	enum class Type
	{
		GamepadUp,
		GamepadDown,
		TextInput
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

		// Text string input (UTF-8)
		// Type: TextInput
		char m_stringUtf8[5];
	};
};

// Event callback for client code.
using EventCallback = void(*)(void* _userData, Event const& _ev);

}