#pragma once
#include <kt/Array.h>
#include <kt/Strings.h>
#include <kt/Slice.h>

#include <shaderlib/CommonShared.h>

#include <gpu/CommandContext.h>
#include <gpu/Types.h>
#include <gpu/HandleRef.h>

#include <gfx/Utils.h>

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
	// TODO: Move rendering code into separate class.
	static gpu::VertexLayout ManualFetchInstancedVertexLayout();

	static uint32_t constexpr c_numShadowCascades = 4;

	Scene();

	void Init(uint32_t _shadowMapResolution = 2048);

	void BeginFrameAndUpdateBuffers(gpu::cmd::Context* _ctx, gfx::Camera const& _mainView, float _dt);

	void RenderInstances(gpu::cmd::Context* _ctx);

	void EndFrame();

	void AddModelInstance(ResourceManager::ModelIdx _idx, kt::Mat4 const& _mtx);

	void BindPerFrameConstants(gpu::cmd::Context* _ctx);

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

	gfx::ResizableDynamicBufferT<gpu::IndexedDrawArguments> m_indirectArgsBuf;
	gfx::ResizableDynamicBufferT<shaderlib::InstanceData_Xform> m_instanceXformBuf;
	gfx::ResizableDynamicBufferT<shaderlib::InstanceData_UniformOffsets> m_instanceUniformsBuf;
	gfx::ResizableDynamicBufferT<uint32_t> m_instanceIdStepBuf;

	gpu::TextureRef m_shadowCascadeTex;

	// TODO: MoveMe.
	gpu::TextureRef m_iblIrradiance;
	gpu::TextureRef m_iblGgx;

	kt::AABB m_sceneBounds;

	kt::Vec3 m_sunColor = kt::Vec3(1.0f);
	float m_sunIntensity = 1.0f;
	kt::Vec2 m_sunThetaPhi = kt::Vec2(0.5f, 0.5f);
};

}