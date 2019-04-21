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

struct CameraController_Free
{
	void SetPos(kt::Vec3 const& _pos);
	void MoveUnscaled(kt::Vec3 const& _movement);
	
	void RotateXY(kt::Vec2 const& _xy);
	void RotateXYZ(kt::Vec3 const& _xyz);
	
	void RotateByQuat(kt::Quat const& _quat);
	void RotateByMatrix(kt::Mat3 const& _mtx);

	void SetRotation(kt::Mat3 const& _rot);
	void SetRotation(kt::Quat const& _rot);

	void UpdateCamera(float _dt, gfx::Camera& _cam);

	bool DefaultInputHandler(input::Event const& _event);
	void HandleGamepadAnalog(input::GamepadState const& _state);

private:
	kt::Vec3 m_keyboardPerFrameDisplacement = kt::Vec3(0.0f);

	kt::Vec3 m_xyzRot = kt::Vec3(0.0f);

	kt::Quat m_rot = kt::Quat::Identity();
	kt::Vec3 m_displacement = kt::Vec3(0.0f);
	kt::Vec3 m_pos = kt::Vec3(0.0f);
};

}