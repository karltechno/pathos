#include <string>

#include <gpu/Types.h>
#include <shaderlib/DefinesShared.h>

#include <kt/Sort.h>
#include <kt/Hash.h>
#include <kt/HashMap.h>
#include <kt/FilePath.h>
#include <kt/Serialization.h>

#include "Scene.h"
#include "Model.h"
#include "Camera.h"
#include "DebugRender.h"
#include "ShadowUtils.h"
#include "Material.h"

namespace gfx
{

uint32_t constexpr c_instanceDataSize = sizeof(float[4*3]);

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

Scene::Scene()
{
	m_lightGpuBuf = CreateLightStructuredBuffer(1024);

	gpu::BufferDesc frameConstDesc;
	frameConstDesc.m_flags = gpu::BufferFlags::Constant | gpu::BufferFlags::Dynamic;
	frameConstDesc.m_sizeInBytes = sizeof(shaderlib::FrameConstants);
	m_frameConstantsGpuBuf = gpu::CreateBuffer(frameConstDesc, nullptr, "Frame Constants Buffer");

	m_frameConstants.time = kt::Vec4(0.0f);
	m_frameConstants.sunColor = kt::Vec3(1.0f);

	gpu::BufferDesc instanceBufferDesc;
	instanceBufferDesc.m_flags = gpu::BufferFlags::Vertex | gpu::BufferFlags::Transient; // TODO: Should instance data be copied out of upload heap?
	instanceBufferDesc.m_strideInBytes = c_instanceDataSize;
	m_instanceGpuBuf = gpu::CreateBuffer(instanceBufferDesc, nullptr, "gfx::Scene instance data");
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
		gpu::BufferDesc oldDesc;
		gpu::ResourceType ty;
		gpu::GetResourceInfo(_scene->m_lightGpuBuf, ty, &oldDesc);
		uint32_t const oldLightSize = oldDesc.m_sizeInBytes / sizeof(shaderlib::LightData);
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
		gpu::ResourceType f;
		gpu::GetResourceInfo(m_shadowCascadeTex, f, nullptr, &desc);
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

struct KT_ALIGNAS(16) InstanceData
{
	InstanceData()
		: m_transform()
	{
	}

	union
	{
		float m_mtx43[4 * 3];

		struct
		{
			// rotation and uniform scale
			kt::Mat3 m_mtx;
			kt::Vec3 m_pos;
		} m_transform;
	};

	ResourceManager::MeshIdx m_meshIdx;

	uint8_t __pad0__[sizeof(m_transform) - sizeof(ResourceManager::MeshIdx)];
};

static void PackMat44_to_Mat43(kt::Mat4 const& _mat4, float* o_mat43)
{
	float const* mtxPtr = _mat4.Data();

	for (uint32_t col = 0; col < 4; ++col)
	{
		o_mat43[col * 3 + 0] = mtxPtr[col * 4 + 0];
		o_mat43[col * 3 + 1] = mtxPtr[col * 4 + 1];
		o_mat43[col * 3 + 2] = mtxPtr[col * 4 + 2];
	}
}

void BuildMeshInstanceArray(kt::Slice<Scene::ModelInstance> const& _modelInstances, kt::Array<InstanceData>& o_instances)
{
	// TODO: Temp allocator, better size.
	o_instances.Reserve(_modelInstances.Size() * 4);

	// TODO: Culling
	// TODO: Slow
	for (Scene::ModelInstance const& modelInstance : _modelInstances)
	{
		gfx::Model const& model = *ResourceManager::GetModel(modelInstance.m_modelIdx);

		InstanceData* instances = o_instances.PushBack_Raw(model.m_nodes.Size());
		// TODO: Bad name?
		for (gfx::Model::Node const& modelMeshInstance : model.m_nodes)
		{
			ResourceManager::MeshIdx const meshIdx = model.m_meshes[modelMeshInstance.m_internalMeshIdx];
			static_assert(sizeof(instances->m_transform) == sizeof(float[4*3]), "Bad instance data size");
			instances->m_meshIdx = meshIdx;
			PackMat44_to_Mat43(kt::Mul(modelInstance.m_mtx, modelMeshInstance.m_mtx), instances->m_mtx43);
			++instances;
		}

	}
}

void Scene::RenderInstances(gpu::cmd::Context* _ctx, bool _shadowMap)
{
	if (m_modelInstances.Size() == 0)
	{
		return;
	}

	kt::Array<InstanceData> instancesSorted;
	BuildMeshInstanceArray(kt::MakeSlice(m_modelInstances.Begin(), m_modelInstances.End()), instancesSorted);

	// TODO: Temp allocator here
	// +1 for sentinel
	instancesSorted.PushBack().m_meshIdx = ResourceManager::MeshIdx{};

	{
		// TODO: big if we actually render lots of instance, again - temp allocator.
		InstanceData* tmp = (InstanceData*)KT_ALLOCA(sizeof(InstanceData) * instancesSorted.Size());
		// -1, don't sort sentinel
		kt::RadixSort(instancesSorted.Begin(), instancesSorted.End() - 1, tmp, [](InstanceData const& _inst) { return _inst.m_meshIdx.idx; });
	}

	uint8_t* instanceStream = gpu::cmd::BeginUpdateTransientBuffer(_ctx, m_instanceGpuBuf, sizeof(InstanceData) * instancesSorted.Size()).Data();

	InstanceData const* begin = instancesSorted.Begin();
	InstanceData const* end = instancesSorted.End() - 1; // -1 for sentinel

	gpu::cmd::SetVertexBuffer(_ctx, _shadowMap ? 1 : 3, m_instanceGpuBuf);
	
	uint32_t batchInstanceBegin = 0;
	for (;;)
	{
		ResourceManager::MeshIdx const curMeshIdx = begin->m_meshIdx;
		
		uint32_t numInstances = 0;

		do 
		{
			memcpy(instanceStream, begin->m_mtx43, c_instanceDataSize);
			instanceStream += c_instanceDataSize;

			++numInstances;
			++begin;
		} while (begin->m_meshIdx == curMeshIdx);

		// Write out draw calls
		gfx::Mesh const& mesh = *ResourceManager::GetMesh(curMeshIdx);
		gpu::cmd::SetVertexBuffer(_ctx, 0, mesh.m_posGpuBuf);
		gpu::cmd::SetIndexBuffer(_ctx, mesh.m_indexGpuBuf);

		if (!_shadowMap)
		{
			gpu::cmd::SetVertexBuffer(_ctx, 1, mesh.m_tangentGpuBuf);
			gpu::cmd::SetVertexBuffer(_ctx, 2, mesh.m_uv0GpuBuf);
		}

		uint32_t lastMaterialIdx = UINT32_MAX;
		for (gfx::Mesh::SubMesh const& subMesh : mesh.m_subMeshes)
		{
			if (!_shadowMap)
			{
				if (subMesh.m_materialIdx.idx != lastMaterialIdx)
				{
					lastMaterialIdx = subMesh.m_materialIdx.idx;
					gpu::DescriptorData cbvBatch;
					cbvBatch.Set(&lastMaterialIdx, sizeof(lastMaterialIdx));
					gpu::cmd::SetGraphicsCBVTable(_ctx, cbvBatch, PATHOS_PER_BATCH_SPACE);
				}
			}

			gpu::cmd::DrawIndexedInstanced(_ctx, subMesh.m_numIndices, numInstances, subMesh.m_indexBufferStartOffset, 0, batchInstanceBegin);
		}
		
		batchInstanceBegin += numInstances;

		if (begin == end)
		{
			break;
		}
	}

	gpu::cmd::EndUpdateTransientBuffer(_ctx, m_instanceGpuBuf);
}

void Scene::EndFrame()
{
	
	gpu::DescriptorData cbv;
	cbv.Set(m_frameConstantsGpuBuf);
	gpu::cmd::Context* ctx = gpu::GetMainThreadCommandCtx();
	gpu::cmd::SetGraphicsCBVTable(ctx, cbv, 1);
	gfx::DebugRender::Flush(gpu::GetMainThreadCommandCtx());
}


}