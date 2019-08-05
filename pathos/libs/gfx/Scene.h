#pragma once
#include <kt/Array.h>
#include <kt/Strings.h>
#include <kt/Slice.h>

#include <shaderlib/CommonShared.h>

#include <gpu/CommandContext.h>
#include <gpu/Types.h>
#include <gpu/HandleRef.h>

#include "Camera.h"
#include "Texture.h"
#include "ResourceManager.h"


namespace gfx
{

struct Model;
struct Camera;

struct Light
{
	enum class Type : uint8_t
	{
		Point,
		Spot,

		Count
	};

	Type m_type = Type::Point;
	kt::Vec3 m_colour = kt::Vec3(1.0f);
	float m_intensity = 1.0f;
	float m_radius = 1.0f;
	float m_spotInnerAngle = 0.0f;
	float m_spotOuterAngle = 0.0f;
	kt::Mat4 m_transform = kt::Mat4::Identity();
};

class Scene
{
public:
	static uint32_t constexpr c_numShadowCascades = 4;

	Scene();

	void Init(uint32_t _shadowMapResolution = 2048);

	void BeginFrameAndUpdateBuffers(gpu::cmd::Context* _ctx, gfx::Camera const& _mainView, float _dt);

	// TODO: Shadow map bool hack.
	void RenderInstances(gpu::cmd::Context* _ctx, bool _shadowMap);

	void EndFrame();

	struct ModelInstance
	{
		kt::Mat4 m_mtx;
		ResourceManager::ModelIdx m_modelIdx;
	};

	kt::Array<ModelInstance> m_modelInstances;

	gfx::Camera m_shadowCascades[c_numShadowCascades];

	kt::Array<Light> m_lights;
	gpu::BufferRef m_lightGpuBuf;

	shaderlib::FrameConstants m_frameConstants;
	gpu::BufferRef m_frameConstantsGpuBuf;

	gpu::BufferRef m_instanceGpuBuf;

	gpu::TextureRef m_shadowCascadeTex;

	kt::AABB m_sceneBounds;

	kt::Vec3 m_sunColor = kt::Vec3(1.0f);
	float m_sunIntensity = 1.0f;
	kt::Vec2 m_sunThetaPhi = kt::Vec2(0.5f, 0.5f);
};

}