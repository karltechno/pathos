#include "Camera.h"

namespace gfx
{



void Camera::SetProjection(ProjectionParams const& _params)
{
	m_projParams = _params;
	if (_params.m_type == ProjType::Orthographic)
	{
		kt::Mat4::OrthographicLH_ZO(-_params.m_viewWidth * 0.5f, _params.m_viewWidth * 0.5f, _params.m_viewHeight * 0.5f, -_params.m_viewHeight * 0.5f, _params.m_nearPlane, _params.m_farPlane);
	}
	else
	{
		KT_ASSERT(_params.m_type == ProjType::Perspective);
		m_viewToClip = kt::Mat4::PerspectiveLH_ZO(_params.m_fov, _params.m_viewWidth / _params.m_viewHeight, _params.m_nearPlane, _params.m_farPlane);
		UpdateCachedWorldToClip();
	}
}

void Camera::SetCameraPos(kt::Vec3 const& _pos)
{
	m_invView.m_cols[3] = kt::Vec4(_pos, 1.0f);
	m_view = kt::InverseOrthoAffine(m_invView);
	UpdateCachedWorldToClip();
}

void Camera::SetCameraMatrix(kt::Mat4 const& _viewToWorld)
{
	m_invView = _viewToWorld;
	m_view = kt::InverseOrthoAffine(m_invView);
	UpdateCachedWorldToClip();
}

kt::Vec3 Camera::GetPos() const
{
	kt::Vec4 pos = m_invView.m_cols[3];
	return kt::Vec3(pos.x, pos.y, pos.z);
}

void Camera::UpdateCachedWorldToClip()
{
	m_cachedWorldToClip = m_viewToClip * m_view;
}


}