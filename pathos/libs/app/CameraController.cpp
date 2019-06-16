#include "CameraController.h"

#include <gfx/Camera.h>
#include <input/InputTypes.h>
#include <input/Input.h>

#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include <kt/Logging.h>

#include <core/CVar.h>

namespace app
{
static core::CVar<float> s_mouseSpeed("cam.mouse_speed", "Mouse rotation speed.", 0.1f, 0.001f, 10.0f);

static core::CVar<float> s_padRotSpeed("cam.pad_rot_speed", "Gamepad rotation speed.", 4.0f, 0.1f, 25.0f);
static core::CVar<float> s_padMoveSpeed("cam.pad_move_speed", "Gamepad movement speed.", 4.0f, 0.1f, 25.0f);
static core::CVar<float> s_dampConstant("cam.damp_constant", "Dampening constant (lambda in exp decay).", 20.0f, 5.0f, 100.0f);


static float Damp(float _a, float _b, float _constant, float _dt)
{
	// Apply an exponential decay.
	return kt::Lerp(_a, _b, 1.0f - expf(-_constant * _dt));
}

static float ClampYaw(float _yaw)
{
	// Keep yaw in [0, PI) and (-PI, 0]
	if (_yaw > kt::kPi)
	{
		return _yaw - 2.0f * kt::kPi;
	}
	else if (_yaw < -kt::kPi)
	{
		return _yaw + 2.0f * kt::kPi;
	}
	return _yaw;
}

static float ClampPitch(float _pitch)
{
	return kt::Clamp(_pitch, -(kt::kPiOverTwo - 0.01f), (kt::kPiOverTwo - 0.01f));
}

void CameraController::UpdateCamera(float _dt, gfx::Camera& _cam)
{
	input::GamepadState pad;
	kt::Vec3 gamepadDisplacement = kt::Vec3(0.0f);

	if (input::GetGamepadState(0, pad))
	{
		if (!!(pad.m_buttonsPressed & input::GamepadButton::RightShoulder))
		{
			s_padMoveSpeed.Set(s_padMoveSpeed * 2.0f);
		}
		if (!!(pad.m_buttonsPressed & input::GamepadButton::LeftShoulder))
		{
			s_padMoveSpeed.Set(s_padMoveSpeed * 0.5f);
		}


		gamepadDisplacement.x += pad.m_leftThumb[0] * s_padMoveSpeed;
		gamepadDisplacement.z += pad.m_leftThumb[1] * s_padMoveSpeed;

		gamepadDisplacement.y += pad.m_rightTrigger * s_padMoveSpeed;
		gamepadDisplacement.y -= pad.m_leftTrigger * s_padMoveSpeed;

		m_frameYaw += pad.m_rightThumb[0];
		m_framePitch -= pad.m_rightThumb[1];
	}

	m_framePitch = -m_framePitch;
	
	kt::Vec3 totalDisplacement = (gamepadDisplacement + m_keyboardPerFrameDisplacement * s_padMoveSpeed);
	totalDisplacement.z = -totalDisplacement.z;

	m_prevYawAnalog = Damp(m_prevYawAnalog, m_frameYaw, s_dampConstant, _dt);
	m_prevPitchAnalog = Damp(m_prevPitchAnalog, m_framePitch, s_dampConstant, _dt);

	m_prevDisplacement.x = Damp(m_prevDisplacement.x, totalDisplacement.x, s_dampConstant, _dt);
	m_prevDisplacement.y = Damp(m_prevDisplacement.y, totalDisplacement.y, s_dampConstant, _dt);
	m_prevDisplacement.z = Damp(m_prevDisplacement.z, totalDisplacement.z, s_dampConstant, _dt);

	m_yaw += m_prevYawAnalog * _dt * s_padRotSpeed;
	m_pitch += m_prevPitchAnalog * _dt * s_padRotSpeed;

	m_yaw = ClampYaw(m_yaw);
	m_pitch = ClampPitch(m_pitch);

	m_framePitch = 0.0f;
	m_frameYaw = 0.0f;

	kt::Vec3 front;
	front.x = cosf(m_yaw) * cosf(m_pitch);
	front.y = sinf(m_pitch);
	front.z = sinf(m_yaw) * cosf(m_pitch);

	kt::Mat4 camMtx = kt::InverseOrthoAffine(kt::Mat4::LookAtRH(m_curPos, front, kt::Vec3(0.0f, 1.0f, 0.0f)));

	m_curPos += kt::MulDir(camMtx, m_prevDisplacement) * _dt;
	camMtx.SetPos(m_curPos);

	_cam.SetCameraMatrix(camMtx);
}

bool CameraController::DefaultInputHandler(input::Event const& _event)
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

		case input::Event::Type::MouseMove:
		{
			m_frameYaw += float(_event.m_mouseMove.deltaX) * s_mouseSpeed;
			m_framePitch += float(_event.m_mouseMove.deltaY) * s_mouseSpeed;
			return true;
		} break;

		default:
		{
			return false;
		} break;
	}

	KT_UNREACHABLE;
}


}