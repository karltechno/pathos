#include <string>
#include <intrin.h>

#include <gpu/Types.h>
#include <core/Memory.h>

#include <shaderlib/DefinesShared.h>
#include <shaderlib/CommonShared.h>

#include <kt/Sort.h>
#include <kt/Hash.h>
#include <kt/HashMap.h>
#include <kt/FilePath.h>
#include <kt/Serialization.h>
#include <kt/LinearAllocator.h>

#include "Scene.h"
#include "Model.h"
#include "Camera.h"
#include "DebugRender.h"
#include "ShadowUtils.h"
#include "Material.h"

namespace gfx
{

static kt::AABB CalcSceneBounds(gfx::Scene const& _scene)
{
	if (_scene.m_modelInstances.Size() == 0)
	{
		return kt::AABB{ kt::Vec3(0.0f), kt::Vec3(0.0f) };
	}

	kt::AABB sceneBounds = kt::AABB::FloatMax();

	for (Scene::ModelInstance const& instance : _scene.m_modelInstances)
	{
		gfx::Model const& model = *ResourceManager::GetModel(instance.m_modelIdx);
		kt::AABB const& modelAABB = model.m_boundingBox;
		sceneBounds = kt::Union(modelAABB.Transformed(instance.m_mtx), sceneBounds);
	}

	return sceneBounds;
}

gpu::BufferRef CreateLightStructuredBuffer(uint32_t _capacity)
{
	gpu::BufferDesc lightBufDesc;
	lightBufDesc.m_flags = gpu::BufferFlags::Dynamic | gpu::BufferFlags::ShaderResource;
	lightBufDesc.m_strideInBytes = sizeof(shaderlib::LightData);
	lightBufDesc.m_sizeInBytes = sizeof(shaderlib::LightData) * _capacity;
	return gpu::CreateBuffer(lightBufDesc, nullptr, "Light Data Buffer");
}

gpu::VertexLayout Scene::ManualFetchInstancedVertexLayout()
{
	gpu::VertexLayout layout;
	layout.Add(gpu::Format::R32_Uint, gpu::VertexSemantic::TexCoord, true);
	return layout;
}

Scene::Scene()
{
	m_frameConstants.time = kt::Vec4(0.0f);
	m_frameConstants.sunColor = kt::Vec3(1.0f);

	m_lightGpuBuf = CreateLightStructuredBuffer(1024);

	{
		gpu::BufferDesc frameConstDesc;
		frameConstDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Dynamic;
		frameConstDesc.m_sizeInBytes = sizeof(shaderlib::FrameConstants);
		m_frameConstantsGpuBuf = gpu::CreateBuffer(frameConstDesc, nullptr, "Frame Constants Buffer");
	}

}

void Scene::Init(uint32_t _shadowMapResolution /*= 2048*/)
{
	gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::DepthStencil | gpu::TextureUsageFlags::ShaderResource;
	gpu::TextureDesc desc = gpu::TextureDesc::Desc2D(_shadowMapResolution, _shadowMapResolution, flags, gpu::Format::D32_Float);
	desc.m_arraySlices = c_numShadowCascades;
	m_shadowCascadeTex = gpu::CreateTexture(desc, nullptr, "Shadow Cascades");
}

static void UpdateLights(Scene* _scene)
{
	uint32_t const numLights = _scene->m_lights.Size();

	if (numLights == 0)
	{
		_scene->m_frameConstants.numLights = 0;
		_scene->m_frameConstants.numPointLights = 0;
		_scene->m_frameConstants.numSpotLights = 0;
		return;
	}

	Light* sortedLights = (Light*)KT_ALLOCA(numLights * sizeof(Light));
	memcpy(sortedLights, _scene->m_lights.Data(), _scene->m_lights.Size() * sizeof(Light));

	{
		// TODO: Could partition if light types == 2. 
		Light* radixTemp = (Light*)KT_ALLOCA(_scene->m_lights.Size() * sizeof(Light));
		kt::RadixSort(sortedLights, sortedLights + _scene->m_lights.Size(), radixTemp, [](Light const& _light) { return uint8_t(_light.m_type); });
	}

	// resize light buffer if necessary
	{
		uint32_t const reqLightSize = numLights;
		uint32_t const oldLightSize = gpu::GetBufferNumElements(_scene->m_lightGpuBuf);
		if (oldLightSize < reqLightSize)
		{
			uint32_t const newLightSize = kt::Max<uint32_t>(oldLightSize * 2, reqLightSize);
			_scene->m_lightGpuBuf = CreateLightStructuredBuffer(newLightSize);
		}
	}

	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();
	shaderlib::LightData* gpuLightData = (shaderlib::LightData*)gpu::cmd::BeginUpdateDynamicBuffer(ctx, _scene->m_lightGpuBuf, numLights * sizeof(shaderlib::LightData), 0).Data();
	
	uint32_t lightCounts[uint32_t(Light::Type::Count)] = {};

	for (uint32_t i = 0; i < numLights; ++i)
	{
		Light const& cpuLight = sortedLights[i];
		static_assert(sizeof(gpuLightData->color) == sizeof(cpuLight.m_colour), "Colour size mismatch");
		memcpy(&gpuLightData->color, &cpuLight.m_colour, sizeof(gpuLightData->color));
		memcpy(&gpuLightData->direction, &cpuLight.m_transform.m_cols[2], sizeof(float[3]));
		gpuLightData->intensity = cpuLight.m_intensity;
		gpuLightData->posWS = cpuLight.m_transform.GetPos();
		gpuLightData->rcpRadius = 1.0f / cpuLight.m_radius;

		float const cosOuter = kt::Cos(cpuLight.m_spotOuterAngle);
		gpuLightData->spotParams.x = cosOuter;
		gpuLightData->spotParams.y = 1.0f / (kt::Cos(cpuLight.m_spotInnerAngle) - cosOuter);
		gpuLightData->type = uint32_t(cpuLight.m_type);
		++gpuLightData;

		++lightCounts[uint32_t(cpuLight.m_type)];
	}

	_scene->m_frameConstants.numPointLights = lightCounts[uint32_t(Light::Type::Point)];
	_scene->m_frameConstants.numSpotLights = lightCounts[uint32_t(Light::Type::Spot)];
	_scene->m_frameConstants.numLights = numLights;

	gpu::cmd::EndUpdateDynamicBuffer(ctx, _scene->m_lightGpuBuf);
}

void Scene::BeginFrameAndUpdateBuffers(gpu::cmd::Context* _ctx, gfx::Camera const& _mainView, float _dt)
{
	m_meshRenderer.Clear();

	m_sceneBounds = CalcSceneBounds(*this);

	m_frameConstants.mainViewProj = _mainView.GetViewProj();
	m_frameConstants.mainProj = _mainView.GetProjection();
	m_frameConstants.mainView = _mainView.GetView();
	m_frameConstants.mainInvView = _mainView.GetInverseView();

	m_frameConstants.camPos = _mainView.GetInverseView().GetPos();
	m_frameConstants.camDist = kt::Length(m_frameConstants.camPos);

	float thetaC = kt::Cos(m_sunThetaPhi.x);
	float thetaS = kt::Sin(m_sunThetaPhi.x);
	float phiC = kt::Cos(m_sunThetaPhi.y);
	float phiS = kt::Sin(m_sunThetaPhi.y);

	m_frameConstants.sunDir.x = thetaC * phiS;
	m_frameConstants.sunDir.y = thetaS * phiS;
	m_frameConstants.sunDir.z = phiC;
	
	uint32_t swapchainX, swapchainY;
	gpu::GetSwapchainDimensions(swapchainX, swapchainY);
	m_frameConstants.screenDims = kt::Vec2(float(swapchainX), float(swapchainY));
	m_frameConstants.screenDimsRcp = kt::Vec2(1.0f / float(swapchainX), 1.0f / float(swapchainY));

	m_frameConstants.numLights = m_lights.Size();

	m_frameConstants.time.x += _dt;
	m_frameConstants.time.y += _dt*0.1f;
	m_frameConstants.time.z = _dt;

	m_frameConstants.sunColor = m_sunColor * m_sunIntensity;

	{
		// calculate cascades
		gpu::TextureDesc desc;
		gpu::GetTextureInfo(m_shadowCascadeTex, desc);
		gfx::CalculateShadowCascades(_mainView, m_frameConstants.sunDir, desc.m_width, c_numShadowCascades, m_shadowCascades, &m_frameConstants.cascadeSplits[0]);

		for (uint32_t cascadeIdx = 0; cascadeIdx < c_numShadowCascades; ++cascadeIdx)
		{
			m_frameConstants.cascadeMatrices[cascadeIdx] = kt::Mul(gfx::NDC_To_UV_Matrix(), m_shadowCascades[cascadeIdx].GetViewProj());
		}
	}

	UpdateLights(this);

	gpu::cmd::ResourceBarrier(_ctx, m_lightGpuBuf, gpu::ResourceState::CopyDest);
	gpu::cmd::ResourceBarrier(_ctx, m_frameConstantsGpuBuf, gpu::ResourceState::CopyDest);
	gpu::cmd::FlushBarriers(_ctx);

	gpu::cmd::UpdateDynamicBuffer(_ctx, m_frameConstantsGpuBuf, &m_frameConstants, sizeof(m_frameConstants));

	gpu::cmd::ResourceBarrier(_ctx, m_lightGpuBuf, gpu::ResourceState::ShaderResource);
	gpu::cmd::ResourceBarrier(_ctx, m_frameConstantsGpuBuf, gpu::ResourceState::ConstantBuffer);
}

void Scene::SubmitInstances()
{
	for (Scene::ModelInstance const& modelInstance : m_modelInstances)
	{
		gfx::Model const& model = *ResourceManager::GetModel(modelInstance.m_modelIdx);
		for (gfx::Model::Node const& modelMeshInstance : model.m_nodes)
		{
			ResourceManager::MeshIdx const meshIdx = model.m_meshes[modelMeshInstance.m_internalMeshIdx];
			m_meshRenderer.Submit(meshIdx, kt::Mul(modelInstance.m_mtx, modelMeshInstance.m_mtx));
		}
	}

	m_meshRenderer.BuildMultiDrawBuffers(gpu::GetMainThreadCommandCtx());
}


void Scene::RenderCascadeViews(gpu::cmd::Context* _ctx)
{
	gpu::cmd::SetViewportAndScissorRectFromTexture(_ctx, m_shadowCascadeTex, 0.0f, 1.0f);
	gpu::cmd::ResourceBarrier(_ctx, m_shadowCascadeTex, gpu::ResourceState::DepthStencilTarget);
	gpu::cmd::SetRenderTarget(_ctx, 0, gpu::TextureHandle{});

	// TODO: Hack because barrier handling is bad atm.
	gpu::cmd::FlushBarriers(_ctx);

	for (uint32_t cascadeIdx = 0; cascadeIdx < c_numShadowCascades; ++cascadeIdx)
	{
		gpu::cmd::ClearDepth(_ctx, m_shadowCascadeTex, 1.0f, cascadeIdx);
		gpu::cmd::SetDepthBuffer(_ctx, m_shadowCascadeTex, cascadeIdx);
		gpu::DescriptorData cbv;
		cbv.Set(m_shadowCascades[cascadeIdx].GetViewProj().Data(), sizeof(kt::Mat4));

		gpu::cmd::SetGraphicsCBVTable(_ctx, cbv, PATHOS_PER_VIEW_SPACE);

		m_meshRenderer.RenderInstances(_ctx);
	}

	gpu::cmd::ResourceBarrier(_ctx, m_shadowCascadeTex, gpu::ResourceState::ShaderResource);
}

void Scene::RenderInstances(gpu::cmd::Context* _ctx)
{
	m_meshRenderer.RenderInstances(_ctx);
}

void Scene::EndFrame()
{
	gpu::DescriptorData cbv;
	cbv.Set(m_frameConstantsGpuBuf);
	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();
	gpu::cmd::SetGraphicsCBVTable(ctx, cbv, PATHOS_PER_FRAME_SPACE);
	gfx::DebugRender::Flush(ctx);
}


void Scene::AddModelInstance(ResourceManager::ModelIdx _idx, kt::Mat4 const& _mtx)
{
	ModelInstance& inst = m_modelInstances.PushBack();
	inst.m_modelIdx = _idx;
	inst.m_mtx = _mtx;
}

void Scene::BindPerFrameConstants(gpu::cmd::Context* _ctx)
{
	// See: "shaderlib/GFXPerFrameBindings.hlsli"
	gpu::DescriptorData frameSrvs[9];

	frameSrvs[0].Set(m_iblIrradiance);
	frameSrvs[1].Set(m_iblGgx);
	frameSrvs[2].Set(gfx::ResourceManager::GetSharedResources().m_ggxLut);
	frameSrvs[3].Set(m_lightGpuBuf);
	frameSrvs[4].Set(m_shadowCascadeTex);
	frameSrvs[5].Set(gfx::ResourceManager::GetMaterialGpuBuffer());

	gfx::ResourceManager::UnifiedBuffers const& buffers = gfx::ResourceManager::GetUnifiedBuffers();

	frameSrvs[6].Set(buffers.m_posVertexBuf);
	frameSrvs[7].Set(buffers.m_tangentSpaceVertexBuf);
	frameSrvs[8].Set(buffers.m_uv0VertexBuf);

	gpu::cmd::SetGraphicsSRVTable(_ctx, frameSrvs, PATHOS_PER_FRAME_SPACE);
}

}