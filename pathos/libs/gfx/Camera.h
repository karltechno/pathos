#pragma once
#include <kt/kt.h>
#include <kt/Mat4.h>
#include <kt/Vec4.h>

namespace gfx
{


struct Camera
{
	enum FrustumCorner
	{
		NearLowerLeft,
		NearUpperLeft,
		NearLowerRight,
		NearUpperRight,

		FarLowerLeft,
		FarUpperLeft,
		FarLowerRight,
		FarUpperRight,

		Num_FrustumCorner
	};

	enum FrustumPlane
	{
		Near,
		Far,
		Left,
		Right,
		Top,
		Bottom,

		Num_FrustumPlane
	};

	enum class ProjType
	{
		Perspective,
		Orthographic
	};

	struct ProjectionParams
	{
		ProjType m_type;
		float m_nearPlane;
		float m_farPlane;
		float m_fov;
		float m_viewHeight;
		float m_viewWidth;
	};

	void SetProjection(ProjectionParams const& _params);
	ProjectionParams const& GetProjectionParams() const { return m_projParams; }

	void SetCameraPos(kt::Vec3 const& _pos);
	void SetCameraMatrix(kt::Mat4 const& _viewToWorld);

	kt::Mat4 const& GetCachedViewProj() const { return m_cachedWorldToClip; }
	kt::Mat4 const& GetInverseView() const { return m_invView; }
	kt::Mat4 const& GetView() const { return m_view; }
	kt::Mat4 const& GetProjection() const { return m_viewToClip; }
	kt::Vec3 GetPos() const;

	kt::Vec3 const* GetFrustumCorners() const { return m_frustumCorners; }
	kt::Vec4 const* GetFrustumPlanes() const { return m_frustumPlanes; }

private:
	void UpdateCachedWorldToClip();
	void CalcFrustumCornersAndPlanes();

	ProjectionParams m_projParams;

	kt::Vec3 m_frustumCorners[FrustumCorner::Num_FrustumCorner];
	kt::Vec4 m_frustumPlanes[FrustumPlane::Num_FrustumPlane];

	kt::Mat4 m_invView = kt::Mat4::Identity(); // Camera in world space
	kt::Mat4 m_view = kt::Mat4::Identity(); // Inverse camera
	kt::Mat4 m_viewToClip = kt::Mat4::Identity(); // projection

	kt::Mat4 m_cachedWorldToClip; // world -> view -> proj
};


}