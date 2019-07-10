#pragma once
#include <kt/kt.h>
#include <kt/Mat4.h>

#include <gpu/HandleRef.h>
#include <gpu/Types.h>

namespace gfx
{
struct Camera;

void CalculateShadowCascades
(
	gfx::Camera const& i_cam,
	kt::Vec3 const& _lightDir,
	uint32_t _shadowResolution,
	uint32_t _numCascades,
	gfx::Camera *o_cascades,
	float *o_splitsViewSpace
);

gpu::PSORef CreateShadowMapPSO_Instanced(gpu::Format _depthFormat);

kt::Mat4 const& NDC_To_UV_Matrix();

}