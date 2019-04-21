#include "CameraController.h"

#include <gfx/Camera.h>
#include <input/InputTypes.h>

#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include <kt/Logging.h>

namespace app
{


void CameraController_Free::SetPos(kt::Vec3 const& _pos)
{
	m_displacement = kt::Vec3(0.0f);
	m_pos = _pos;
}

void CameraController_Free::MoveUnscaled(kt::Vec3 const& _movement)
{
	m_displacement += _movement;
}

void CameraController_Free::RotateXY(kt::Vec2 const& _xy)
{
	kt::Quat rotx;
	rotx.FromNormalizedAxisAngle(kt::Vec3(0.0f, 1.0f, 0.0f), _xy.x);
	kt::Quat roty;
	roty.FromNormalizedAxisAngle(kt::Vec3(1.0f, 0.0f, 0.0f), _xy.y);

	// TODO: Is this right
	m_rot = m_rot * roty * rotx;
}

void CameraController_Free::RotateXYZ(kt::Vec3 const& _xyz)
{
	m_xyzRot += _xyz;
}

void CameraController_Free::RotateByQuat(kt::Quat const& _quat)
{
	m_rot = m_rot * _quat;
}

void CameraController_Free::RotateByMatrix(kt::Mat3 const& _mtx)
{
	m_rot = m_rot * kt::ToQuat(_mtx);
}

void CameraController_Free::SetRotation(kt::Mat3 const& _rot)
{
	m_rot = kt::ToQuat(_rot);
}

void CameraController_Free::SetRotation(kt::Quat const& _rot)
{
	m_rot = _rot;
}

void CameraController_Free::UpdateCamera(float _dt, gfx::Camera& _cam)
{
	kt::Quat rotx;
	rotx.FromNormalizedAxisAngle(kt::Vec3(1.0f, 0.0f, 0.0f), m_xyzRot.x * _dt);
	kt::Quat roty;
	roty.FromNormalizedAxisAngle(kt::Vec3(0.0f, 1.0f, 0.0f), m_xyzRot.y * _dt);
	kt::Quat rotz;
	rotz.FromNormalizedAxisAngle(kt::Vec3(0.0f, 0.0f, 1.0f), m_xyzRot.z * _dt);
	m_rot = m_rot * rotz * roty * rotx;

	m_xyzRot = kt::Vec3(0.0f);
	kt::Mat4 mtx = kt::ToMat4(m_rot);
	kt::Vec3 const frameDisplacement = (m_displacement + m_keyboardPerFrameDisplacement) * _dt * 20.0f; // TODO: Multiplier
	m_pos += mtx.m_cols[0] * frameDisplacement.x + mtx.m_cols[1] * frameDisplacement.y + mtx.m_cols[2] * frameDisplacement.z;

	m_displacement = kt::Vec3(0.0f);
	mtx.m_cols[3] = kt::Vec4(m_pos.x, m_pos.y, m_pos.z, 1.0f);
	_cam.SetCameraMatrix(mtx);
}

bool CameraController_Free::DefaultInputHandler(input::Event const& _event)
{
	switch (_event.m_type)
	{
		case input::Event::Type::KeyDown:
		case input::Event::Type::KeyUp:
		{
			float const dis = _event.m_type == input::Event::Type::KeyDown ? 1.0f : -1.0f;

			switch (_event.m_key)
			{
				case input::Key::KeyW: m_keyboardPerFrameDisplacement.z += dis; break;
				case input::Key::KeyS: m_keyboardPerFrameDisplacement.z -= dis; break;

				case input::Key::KeyD: m_keyboardPerFrameDisplacement.x += dis; break;
				case input::Key::KeyA: m_keyboardPerFrameDisplacement.x -= dis; break;
				
				case input::Key::Space: m_keyboardPerFrameDisplacement.y += dis; break;
				case input::Key::KeyZ: m_keyboardPerFrameDisplacement.y -= dis; break;
				default: break;
			}
			return true;
		} break;

		default:
		{
			return false;
		} break;
	}

	KT_UNREACHABLE;
}

void CameraController_Free::HandleGamepadAnalog(input::GamepadState const& _state)
{
	m_displacement.x += _state.m_leftThumb[0];
	m_displacement.z += _state.m_leftThumb[1];

	m_displacement.y += _state.m_rightTrigger;
	m_displacement.y -= _state.m_leftTrigger;

	m_xyzRot.y += _state.m_rightThumb[0];
	m_xyzRot.x -= _state.m_rightThumb[1];
}

}