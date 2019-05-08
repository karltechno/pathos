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

static core::CVar<float> s_padRotSpeed("cam.pad_speed_rot", "Gamepad rotation speed.", 4.0f, 0.1f, 25.0f);
static core::CVar<float> s_padMoveSpeed("cam.pad_rot_sens", "Gamepad movement speed.", 4.0f, 0.1f, 25.0f);


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

		m_frameYaw += pad.m_rightThumb[0] * s_padRotSpeed;
		m_framePitch -= pad.m_rightThumb[1] * s_padRotSpeed;


	}

	m_pitch += m_framePitch * _dt;
	m_pitch = kt::Clamp(m_pitch, -kt::kPiOverTwo, kt::kPiOverTwo);

	m_yaw += m_frameYaw * _dt;
	
	// Keep yaw in [0, PI] and [-PI, 0]
	if (m_yaw >= kt::kPi)
	{
		m_yaw -= 2.0f * kt::kPi;
	}
	else if (m_yaw <= -kt::kPi)
	{
		m_yaw += 2.0f * kt::kPi;
	}

	kt::Vec3 const frameDisplacement = (gamepadDisplacement + m_keyboardPerFrameDisplacement) * _dt * 20.0f; // TODO: Multiplier

	m_framePitch = 0.0f;
	m_frameYaw = 0.0f;

	kt::Mat4 camMtx =  kt::Mat4::RotY(m_yaw) * kt::Mat4::RotX(m_pitch);

	m_pos += kt::MulDir(camMtx, frameDisplacement);

	camMtx.SetPos(m_pos);

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
			m_frameYaw += float(_event.m_mouseMove.deltaX);
			m_framePitch += float(_event.m_mouseMove.deltaY);
		} break;

		default:
		{
			return false;
		} break;
	}

	KT_UNREACHABLE;
}


}