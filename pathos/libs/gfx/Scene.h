#pragma once
#include <kt/Array.h>
#include <kt/Strings.h>
#include <kt/Slice.h>

#include <shaderlib/CommonShared.h>
#include <shaderlib/LightingStructs.h>

#include <gpu/CommandContext.h>
#include <gpu/HandleRef.h>

#include "Camera.h"


namespace gfx
{

struct Model;
struct Camera;

class Scene
{
public:
	static uint32_t constexpr c_numShadowCascades = 4;

	static gfx::Model* CreateModel(char const* _name);
	static void SetModelName(gfx::Model* _m, char const* _name);

	// Free allocated models, if you care :)
	static void Shutdown();

	Scene();

	void Init(uint32_t _shadowMapResolution = 2048);

	void BeginFrameAndUpdateBuffers(gpu::cmd::Context* _ctx, gfx::Camera const& _mainView, float _dt);

	// TODO: Shadow map bool hack.
	void RenderInstances(gpu::cmd::Context* _ctx, bool _shadowMap);

	void EndFrame();

	struct LoadedModel
	{
		kt::String64 m_name;
		gfx::Model* m_model;
	};

	struct KT_ALIGNAS(16) InstanceData
	{
		InstanceData()
			: m_transform()
		{
		}

		union
		{
			float m_data[4 * 3];

			struct
			{
				// rotation and uniform scale
				kt::Mat3 m_mtx;
				kt::Vec3 m_pos;
			} m_transform;
		};
		uint32_t m_modelIdx;
		uint32_t __pad0__[3];
	};


	// Convenient global list of models.
	static kt::Array<kt::String64> s_modelNames;
	static kt::Array<gfx::Model*> s_models;

	gfx::Camera m_shadowCascades[c_numShadowCascades];

	kt::Array<shaderlib::LightData> m_lights;
	gpu::BufferRef m_lightGpuBuf;

	shaderlib::FrameConstants m_frameConstants;
	gpu::BufferRef m_frameConstantsGpuBuf;

	kt::Array<InstanceData> m_instances;
	gpu::BufferRef m_instanceGpuBuf;

	gpu::TextureRef m_shadowCascadeTex;

	kt::AABB m_sceneBounds;
};

}