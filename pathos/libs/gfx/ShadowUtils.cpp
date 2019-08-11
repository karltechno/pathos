#include "ShadowUtils.h"
#include "Camera.h"
#include "DebugRender.h"

#include <core/CVar.h>

#include <kt/AABB.h>
#include <kt/MathUtil.h>

#include "Scene.h"


namespace gfx
{
static core::CVar<float> s_csmLambda("gfx.shadows.csm_lambda", "Interpolation constant for log/linear csm splits.", 0.65f, 0.0f, 1.0f);

void CalculateShadowCascades
(
	Camera const& i_cam, 
	kt::Vec3 const& _lightDir, 
	uint32_t _shadowResolution, 
	uint32_t _numCascades,
	Camera *o_cascades, 
	float *o_splitsViewSpace
)
{
	Camera::ProjectionParams const& viewParams = i_cam.GetProjectionParams();

	float const near = viewParams.m_nearPlane;
	float const far = viewParams.m_farPlane;

	float const farNearRatio = far / near;
	float const range = far - near;

	float* splitsT = (float*)KT_ALLOCA(_numCascades * sizeof(float));

	float const c_pssmLambda = s_csmLambda;

	for (uint32_t cascadeIdx = 0; cascadeIdx < _numCascades; ++cascadeIdx)
	{
		// Practical split scheme described in: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		
		float const t = (cascadeIdx + 1) / float(_numCascades);
		float const cLogarithmic = near * powf(farNearRatio, t);
		float const cUniform = near + range * t;

		float const cascadeSplit = kt::Lerp(cUniform, cLogarithmic, c_pssmLambda);

		// Store the splits as the actual view space depth for purposes of calculating the cascade in shader
		o_splitsViewSpace[cascadeIdx] = cascadeSplit;

		// And normalized [0, 1] for cascade matrix calculation.
		splitsT[cascadeIdx] = (cascadeSplit - near) / range;
	}

	kt::Vec3 const* frustumCorners = i_cam.GetFrustumCorners();

	kt::Vec3 const lowerLeftRay = frustumCorners[gfx::Camera::FrustumCorner::FarLowerLeft] - frustumCorners[gfx::Camera::FrustumCorner::NearLowerLeft];
	kt::Vec3 const upperLeftRay = frustumCorners[gfx::Camera::FrustumCorner::FarUpperLeft] - frustumCorners[gfx::Camera::FrustumCorner::NearUpperLeft];
	kt::Vec3 const lowerRightRay = frustumCorners[gfx::Camera::FrustumCorner::FarLowerRight] - frustumCorners[gfx::Camera::FrustumCorner::NearLowerRight];
	kt::Vec3 const upperRightRay = frustumCorners[gfx::Camera::FrustumCorner::FarUpperRight] - frustumCorners[gfx::Camera::FrustumCorner::NearUpperRight];

	kt::Vec3 prevCascadeCorners[gfx::Camera::Num_FrustumCorner / 2];
	// Near corners stored first.
	memcpy(prevCascadeCorners, frustumCorners, sizeof(prevCascadeCorners));

	for (uint32_t cascadeIdx = 0; cascadeIdx < _numCascades; ++cascadeIdx)
	{
		// Interpolate the new corners
		float const cascadeT = splitsT[cascadeIdx];
		kt::Vec3 cascadeCorners[gfx::Camera::Num_FrustumCorner / 2];
		cascadeCorners[0] = lowerLeftRay * cascadeT + frustumCorners[gfx::Camera::FrustumCorner::NearLowerLeft];
		cascadeCorners[1] = upperLeftRay * cascadeT + frustumCorners[gfx::Camera::FrustumCorner::NearUpperLeft];
		cascadeCorners[2] = lowerRightRay * cascadeT + frustumCorners[gfx::Camera::FrustumCorner::NearLowerRight];
		cascadeCorners[3] = upperRightRay * cascadeT + frustumCorners[gfx::Camera::FrustumCorner::NearUpperRight];

		kt::Vec3 frustumCenter = cascadeCorners[0] + cascadeCorners[1] + cascadeCorners[2] + cascadeCorners[3];
		frustumCenter += prevCascadeCorners[0] + prevCascadeCorners[1] + prevCascadeCorners[2] + prevCascadeCorners[3];
		frustumCenter /= 8.0f;

		kt::AABB frustumAabbLightSpace = kt::AABB::FloatMax();
		

		{
			// Calculate bounding sphere to be rotationally invariant.
			float sphRad = 0.0f;
			for (uint32_t cornerIdx = 0; cornerIdx < 4; ++cornerIdx)
			{
				kt::Vec3 const& p1 = cascadeCorners[cornerIdx];
				kt::Vec3 const& p2 = prevCascadeCorners[cornerIdx];
				sphRad = kt::Max(sphRad, kt::Length(p1 - frustumCenter));
				sphRad = kt::Max(sphRad, kt::Length(p2 - frustumCenter));
			}
			frustumAabbLightSpace.m_min = kt::Vec3(-sphRad );
			frustumAabbLightSpace.m_max = kt::Vec3( sphRad );
		}

		gfx::Camera::ProjectionParams cascadeProjParams;
		float const farCam = frustumAabbLightSpace.m_max.z - frustumAabbLightSpace.m_min.z;
		kt::Vec3 const shadowCamPos = frustumCenter + _lightDir * frustumAabbLightSpace.m_min.z;

		o_cascades[cascadeIdx].SetView(kt::Mat4::LookAtRH(shadowCamPos, _lightDir));	
		cascadeProjParams.SetOrtho(0.0f, farCam, frustumAabbLightSpace.m_min.x, frustumAabbLightSpace.m_max.x, frustumAabbLightSpace.m_max.y, frustumAabbLightSpace.m_min.y);
		o_cascades[cascadeIdx].SetProjection(cascadeProjParams);

		// Derive a matrix to remove sub-pixel movement by snapping to texel-sized increments.
		kt::Vec4 const originInShadowSpace = kt::Mul(o_cascades[cascadeIdx].GetViewProj(), kt::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
		kt::Vec4 const projectedTexelPos = originInShadowSpace * float(_shadowResolution) * 0.5f;
		kt::Vec2 const roundOff = kt::Vec2(floorf(projectedTexelPos.x) - projectedTexelPos.x, floorf(projectedTexelPos.y) - projectedTexelPos.y) / (float(_shadowResolution) * 0.5f);
		kt::Mat4 const roundingMatrix = kt::Mat4::Translation(kt::Vec3(roundOff.x, roundOff.y, 0.0f));

		o_cascades[cascadeIdx].SetProjection(gfx::Camera::ProjType::Orthographic, kt::Mul(roundingMatrix, o_cascades[cascadeIdx].GetProjection()));

		// Copy current cascades back to previous.
		memcpy(prevCascadeCorners, cascadeCorners, sizeof(cascadeCorners));
	}
}


gpu::PSORef CreateShadowMapPSO(gpu::Format _depthFormat)
{
	gpu::GraphicsPSODesc desc;
	desc.m_depthFormat = _depthFormat;
	desc.m_numRenderTargets = 0;
	desc.m_primType = gpu::PrimitiveType::TriangleList;
	desc.m_vertexLayout.Add(gpu::Format::R32_Uint, gpu::VertexSemantic::TexCoord, true, 0, 0);

	desc.m_depthStencilDesc.m_depthFn = gpu::ComparisonFn::Less;
	desc.m_depthStencilDesc.m_depthWrite = true;
	desc.m_depthStencilDesc.m_depthEnable = true;

	desc.m_rasterDesc.m_scopedScaledDepthBias = 3.0f;

	gpu::ShaderRef const vs = ResourceManager::LoadShader("shaders/ShadowMap.vs.cso", gpu::ShaderType::Vertex);

	desc.m_vs = vs;
	return gpu::CreateGraphicsPSO(desc, "Shadow Map PSO");
}

static kt::Mat4 const s_ndc_to_uv =
{
	kt::Vec4 { 0.5f, 0.0f, 0.0f, 0.0f },
	kt::Vec4 { 0.0f, -0.5f, 0.0f, 0.0f },
	kt::Vec4 { 0.0f, 0.0f, 1.0f, 0.0f },
	kt::Vec4 { 0.5f, 0.5f, 0.0f, 1.0f }
};

kt::Mat4 const& NDC_To_UV_Matrix()
{
	// Matrix transforming NDC [-1, 1] to UV [0, 1]. Also flipping Y since [0, 0] is upper left in UV space.
	return s_ndc_to_uv;
}

}

