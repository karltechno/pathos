#include "Scene.h"
#include "Model.h"
#include "Camera.h"

namespace gfx
{

kt::Array<kt::String64> Scene::s_modelNames;
kt::Array<gfx::Model*> Scene::s_models;

gpu::BufferRef CreateLightStructuredBuffer(uint32_t _capacity)
{
	gpu::BufferDesc lightBufDesc;
	lightBufDesc.m_flags = gpu::BufferFlags::Dynamic | gpu::BufferFlags::ShaderResource;
	lightBufDesc.m_strideInBytes = sizeof(shaderlib::LightData);
	lightBufDesc.m_sizeInBytes = sizeof(shaderlib::LightData) * _capacity;
	return gpu::CreateBuffer(lightBufDesc, nullptr, "Light Data Buffer");
}

Scene::Scene()
{
	m_lightGpuBuf = CreateLightStructuredBuffer(1024);

	gpu::BufferDesc frameConstDesc;
	frameConstDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Dynamic;
	frameConstDesc.m_sizeInBytes = sizeof(shaderlib::FrameConstants);
	m_frameConstantsGpuBuf = gpu::CreateBuffer(frameConstDesc, nullptr, "Frame Constants Buffer");

	m_frameConstants.time = kt::Vec4(0.0f);
	m_frameConstants.sunColor = kt::Vec3(1.0f);
	m_frameConstants.sunDir = kt::Vec3(0.0f, -1.0f, 0.0f);
}

void Scene::UpdateFrameData(gpu::cmd::Context* _ctx, gfx::Camera const& _mainView, float _dt)
{
	m_frameConstants.mainViewProj = _mainView.GetCachedViewProj();
	m_frameConstants.mainProj = _mainView.GetProjection();
	m_frameConstants.mainView = _mainView.GetView();
	m_frameConstants.mainInvView = _mainView.GetInverseView();

	m_frameConstants.camPos = _mainView.GetInverseView().GetPos();
	m_frameConstants.camDist = kt::Length(m_frameConstants.camPos);
	
	uint32_t swapchainX, swapchainY;
	gpu::GetSwapchainDimensions(swapchainX, swapchainY);
	m_frameConstants.screenDims = kt::Vec2(float(swapchainX), float(swapchainY));
	m_frameConstants.screenDimsRcp = kt::Vec2(1.0f / float(swapchainX), 1.0f / float(swapchainY));

	m_frameConstants.numLights = m_lights.Size();

	m_frameConstants.time.x += _dt;
	m_frameConstants.time.y += _dt*0.1f;
	m_frameConstants.time.z = _dt;

	// resize light buffer if necessary
	uint32_t const reqLightSize = sizeof(shaderlib::LightData) * m_lights.Size();
	gpu::BufferDesc oldDesc;
	gpu::ResourceType ty;
	gpu::GetResourceInfo(m_lightGpuBuf, ty, &oldDesc);
	uint32_t const oldLightSize = oldDesc.m_sizeInBytes / sizeof(shaderlib::LightData);
	if (oldLightSize < reqLightSize)
	{
		uint32_t const newLightSize = kt::Max<uint32_t>(oldLightSize * 2, reqLightSize);
		m_lightGpuBuf = CreateLightStructuredBuffer(newLightSize);
	}

	gpu::cmd::ResourceBarrier(_ctx, m_lightGpuBuf, gpu::ResourceState::CopyDest);
	gpu::cmd::ResourceBarrier(_ctx, m_frameConstantsGpuBuf, gpu::ResourceState::CopyDest);
	gpu::cmd::FlushBarriers(_ctx);

	gpu::cmd::UpdateDynamicBuffer(_ctx, m_lightGpuBuf, m_lights.Data(), m_lights.Size() * sizeof(shaderlib::LightData));
	gpu::cmd::UpdateDynamicBuffer(_ctx, m_frameConstantsGpuBuf, &m_frameConstants, sizeof(m_frameConstants));

	gpu::cmd::ResourceBarrier(_ctx, m_lightGpuBuf, gpu::ResourceState::ShaderResource);
	gpu::cmd::ResourceBarrier(_ctx, m_frameConstantsGpuBuf, gpu::ResourceState::ConstantBuffer);
}

gfx::Model* Scene::CreateModel(char const* _name)
{
	s_modelNames.PushBack(kt::String64(_name));
	gfx::Model* m = s_models.PushBack(new gfx::Model);
	m->m_globalSceneIndex = s_models.Size() - 1;
	return m;
}

void Scene::SetModelName(gfx::Model* _m, char const* _name)
{
	KT_ASSERT(_m && _m->m_globalSceneIndex < s_models.Size());
	s_modelNames[_m->m_globalSceneIndex] = kt::String64{ _name };
}

void Scene::Shutdown()
{
	for (gfx::Model* m : s_models)
	{
		delete m;
	}

	s_models.ClearAndFree();
	s_modelNames.ClearAndFree();
}


}