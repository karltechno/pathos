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
		void SetOrtho(float _near, float _far, float _height, float _width);
		void SetOrtho(float _near, float _far, float _left, float _right, float _top, float _bottom);

		void SetPerspective(float _near, float _far, float _fov, float _aspect);

		ProjType m_type;
		float m_nearPlane;
		float m_farPlane;
		
		union
		{
			struct  
			{
				float left;
				float right;
				float bottom;
				float top;
			} m_ortho;

			struct  
			{
				float fov;
				float aspect;
			} m_proj;
		};
	};


	ProjectionParams const& GetProjectionParams() const { return m_projParams; }
	void SetProjection(ProjectionParams const& _params);
	
	// TODO: ProjectionParams may be invalid if projection is set manually! (only currently used to append sub-pixel offset for stable csm's)
	void SetProjection(ProjType _type, kt::Mat4 const& _proj);

	void SetCameraPos(kt::Vec3 const& _pos);
	void SetCameraMatrix(kt::Mat4 const& _viewToWorld);
	void SetView(kt::Mat4 const& _worldToView);

	kt::Mat4 const& GetViewProj() const { return m_cachedWorldToClip; }
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