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

	kt::Array<shaderlib::LightData> m_lights;
	gpu::BufferRef m_lightGpuBuf;

	shaderlib::FrameConstants m_frameConstants;
	gpu::BufferRef m_frameConstantsGpuBuf;

	struct LoadedModel
	{
		kt::String64 m_name;
		gfx::Model* m_model;
	};

	struct Instance
	{
		kt::Mat3 m_rot;
		kt::Vec3 m_pos;
		uint32_t m_modelIdx;
	};

	kt::Array<Instance> m_instances;
	
	// Convenient global list of models.
	static kt::Array<kt::String64> s_modelNames;
	static kt::Array<gfx::Model*> s_models;
};

}