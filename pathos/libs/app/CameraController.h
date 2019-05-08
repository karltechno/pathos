#pragma once
#include <kt/kt.h>
#include <kt/Vec3.h>
#include <kt/Quat.h>

namespace gfx
{
struct Camera;
}

namespace input
{
struct Event;
struct GamepadState;
}

namespace app
{

struct CameraController
{
	void UpdateCamera(float _dt, gfx::Camera& _cam);

	bool DefaultInputHandler(input::Event const& _event);

private:
	kt::Vec3 m_keyboardPerFrameDisplacement = kt::Vec3(0.0f);
	float m_frameYaw = 0.0f;
	float m_framePitch = 0.0f;

	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	kt::Vec3 m_pos = kt::Vec3(0.0f);
};

}