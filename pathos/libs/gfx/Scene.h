#pragma once
#include <kt/Array.h>
#include <kt/Strings.h>
#include <kt/Slice.h>

#include <shaderlib/CommonShared.h>
#include <shaderlib/LightingStructs.h>

#include <gpu/CommandContext.h>
#include <gpu/HandleRef.h>


namespace gfx
{

struct Model;
struct Camera;

class Scene
{
public:
	static gfx::Model* CreateModel(char const* _name);
	static void SetModelName(gfx::Model* _m, char const* _name);

	// Free allocated models, if you care :)
	static void Shutdown();

	Scene();

	void UpdateFrameData(gpu::cmd::Context* _ctx, gfx::Camera const& _mainView, float _dt);

	void RenderInstances(gpu::cmd::Context* _ctx);

	kt::Array<shaderlib::LightData> m_lights;
	gpu::BufferRef m_lightGpuBuf;

	shaderlib::FrameConstants m_frameConstants;
	gpu::BufferRef m_frameConstantsGpuBuf;

	struct LoadedModel
	{
		kt::String64 m_name;
		gfx::Model* m_model;
	};

	struct KT_ALIGNAS(16) InstanceData
	{
		InstanceData() 
			: m_transform()
		{}

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

	kt::Array<InstanceData> m_instances;
	gpu::BufferRef m_instanceGpuBuf;

	// Convenient global list of models.
	static kt::Array<kt::String64> s_modelNames;
	static kt::Array<gfx::Model*> s_models;
};

}