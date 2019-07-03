#include "Camera.h"

namespace gfx
{

void Camera::SetProjection(ProjectionParams const& _params)
{
	m_projParams = _params;
	if (_params.m_type == ProjType::Orthographic)
	{
		m_viewToClip = kt::Mat4::OrthographicRH_ZO
		(
			-_params.m_viewWidth * 0.5f, 
			_params.m_viewWidth * 0.5f, 
			_params.m_viewHeight * 0.5f, 
			-_params.m_viewHeight * 0.5f, 
			_params.m_nearPlane,
			_params.m_farPlane
		);
	}
	else
	{
		KT_ASSERT(_params.m_type == ProjType::Perspective);
		m_viewToClip = kt::Mat4::PerspectiveRH_ZO(_params.m_fov, _params.m_viewWidth / _params.m_viewHeight, _params.m_nearPlane, _params.m_farPlane);
	}

	UpdateCachedWorldToClip();
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
	// TODO: This probably gets updated more than it needs to, but whatever.
	m_cachedWorldToClip = m_viewToClip * m_view;
	CalcFrustumCornersAndPlanes();
}


void Camera::CalcFrustumCornersAndPlanes()
{
	// First calculate frustum corners 

	// camera -> world basis vectors
	kt::Vec3 const basisX = kt::Vec3(m_invView[0]);
	kt::Vec3 const basisY = kt::Vec3(m_invView[1]);
	kt::Vec3 const basisZ = kt::Vec3(m_invView[2]);

	// Interpolate Z axis across near/far.
	kt::Vec3 const nearCenter = m_projParams.m_nearPlane * -basisZ;
	kt::Vec3 const farCenter = m_projParams.m_farPlane * -basisZ;

	float nearX, nearY, farX, farY;

	if (m_projParams.m_type == ProjType::Orthographic)
	{
		// Offsets are just screen extents.
		nearX = farX = m_projParams.m_viewWidth * 0.5f;
		nearY = farY = m_projParams.m_viewHeight * 0.5f;
	}
	else
	{
		// Calc offsets with aspect ratio and vertical fov.
		float const aspect = m_projParams.m_viewWidth / m_projParams.m_viewHeight;
		float const cotAsp = kt::Tan(m_projParams.m_fov * 0.5f);

		nearY = cotAsp * m_projParams.m_nearPlane;
		nearX = nearY * aspect;

		farY = cotAsp * m_projParams.m_farPlane;
		farX = farY * aspect;
	}

	// Use center and offsets to position the corners
	m_frustumCorners[FrustumCorner::NearLowerLeft] = nearCenter - basisX * nearX - basisY * nearY;
	m_frustumCorners[FrustumCorner::NearUpperLeft] = nearCenter - basisX * nearX + basisY * nearY;
	m_frustumCorners[FrustumCorner::NearLowerRight] = nearCenter + basisX * nearX - basisY * nearY;
	m_frustumCorners[FrustumCorner::NearUpperRight] = nearCenter + basisX * nearX + basisY * nearY;
	m_frustumCorners[FrustumCorner::FarLowerLeft] = farCenter - basisX * farX - basisY * farY;
	m_frustumCorners[FrustumCorner::FarUpperLeft] = farCenter - basisX * farX + basisY * farY;
	m_frustumCorners[FrustumCorner::FarLowerRight] = farCenter + basisX * farX - basisY * farY;
	m_frustumCorners[FrustumCorner::FarUpperRight] = farCenter + basisX * farX + basisY * farY;


	auto normalizePlane = [](kt::Vec4& _p) { float const len = kt::Sqrt(_p.x*_p.x + _p.y*_p.y + _p.z*_p.z); _p /= len; };

	// Use equations formulated from clip space inequalities.

	kt::Mat4 const& m = m_cachedWorldToClip;
	
	m_frustumPlanes[FrustumPlane::Near].x = m[3][0] + m[2][0];
	m_frustumPlanes[FrustumPlane::Near].y = m[3][1] + m[2][1];
	m_frustumPlanes[FrustumPlane::Near].z = m[3][2] + m[2][2];
	m_frustumPlanes[FrustumPlane::Near].w = m[3][3] + m[2][3];
	normalizePlane(m_frustumPlanes[FrustumPlane::Near]);

	m_frustumPlanes[FrustumPlane::Far].x = m[3][0] - m[2][0];
	m_frustumPlanes[FrustumPlane::Far].y = m[3][1] - m[2][1];
	m_frustumPlanes[FrustumPlane::Far].z = m[3][2] - m[2][2];
	m_frustumPlanes[FrustumPlane::Far].w = m[3][3] - m[2][3];
	normalizePlane(m_frustumPlanes[FrustumPlane::Far]);
	
	m_frustumPlanes[FrustumPlane::Left].x = m[3][0] + m[0][0];
	m_frustumPlanes[FrustumPlane::Left].y = m[3][1] + m[0][1];
	m_frustumPlanes[FrustumPlane::Left].z = m[3][2] + m[0][2];
	m_frustumPlanes[FrustumPlane::Left].w = m[3][3] + m[0][3];
	normalizePlane(m_frustumPlanes[FrustumPlane::Left]);

	m_frustumPlanes[FrustumPlane::Right].x = m[3][0] - m[0][0];
	m_frustumPlanes[FrustumPlane::Right].y = m[3][1] - m[0][1];
	m_frustumPlanes[FrustumPlane::Right].z = m[3][2] - m[0][2];
	m_frustumPlanes[FrustumPlane::Right].w = m[3][3] - m[0][3];
	normalizePlane(m_frustumPlanes[FrustumPlane::Right]);

	m_frustumPlanes[FrustumPlane::Top].x = m[3][0] - m[1][0];
	m_frustumPlanes[FrustumPlane::Top].y = m[3][1] - m[1][1];
	m_frustumPlanes[FrustumPlane::Top].z = m[3][2] - m[1][2];
	m_frustumPlanes[FrustumPlane::Top].w = m[3][3] - m[1][3];
	normalizePlane(m_frustumPlanes[FrustumPlane::Top]);

	m_frustumPlanes[FrustumPlane::Bottom].x = m[3][0] + m[1][0];
	m_frustumPlanes[FrustumPlane::Bottom].y = m[3][1] + m[1][1];
	m_frustumPlanes[FrustumPlane::Bottom].z = m[3][2] + m[1][2];
	m_frustumPlanes[FrustumPlane::Bottom].w = m[3][3] + m[1][3];
	normalizePlane(m_frustumPlanes[FrustumPlane::Bottom]);
}

}