#include "Scene.h"
#include "Model.h"
#include "Camera.h"
#include "DebugRender.h"
#include "ShadowUtils.h"

#include <res/ResourceSystem.h>
#include <gpu/Types.h>

#include <kt/Sort.h>

namespace gfx
{

kt::Array<kt::String64> Scene::s_modelNames;
kt::Array<gfx::Model*> Scene::s_models;

uint32_t constexpr c_indexDataSize = sizeof(float) * 4 * 3;

static kt::AABB CalcSceneBounds(gfx::Scene const& _scene)
{
	if (_scene.m_instances.Size() == 0)
	{
		return kt::AABB{ kt::Vec3(0.0f), kt::Vec3(0.0f) };
	}

	kt::AABB sceneBounds = kt::AABB::FloatMax();

	for (Scene::InstanceData const& instance : _scene.m_instances)
	{
		kt::AABB const& modelAABB = Scene::s_models[instance.m_modelIdx]->m_boundingBox;
		sceneBounds = kt::Union(modelAABB.Transformed(instance.m_transform.m_mtx, instance.m_transform.m_pos), sceneBounds);
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
	m_frameConstants.sunDir = kt::Normalize(kt::Vec3(0.4f, -1.0f, 0.15f));

	gpu::BufferDesc instanceBufferDesc;
	instanceBufferDesc.m_flags = gpu::BufferFlags::Vertex | gpu::BufferFlags::Transient; // TODO: Should instance data be copied out of upload heap?
	instanceBufferDesc.m_strideInBytes = c_indexDataSize;
	m_instanceGpuBuf = gpu::CreateBuffer(instanceBufferDesc, nullptr, "gfx::Scene instance data");
}

void Scene::Init(uint32_t _shadowMapResolution /*= 2048*/)
{
	gpu::TextureUsageFlags const flags = gpu::TextureUsageFlags::DepthStencil | gpu::TextureUsageFlags::ShaderResource;
	gpu::TextureDesc desc = gpu::TextureDesc::Desc2D(_shadowMapResolution, _shadowMapResolution, flags, gpu::Format::D32_Float);
	desc.m_arraySlices = c_numShadowCascades;
	m_shadowCascadeTex = gpu::CreateTexture(desc, nullptr, "Shadow Cascades");
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
	
	uint32_t swapchainX, swapchainY;
	gpu::GetSwapchainDimensions(swapchainX, swapchainY);
	m_frameConstants.screenDims = kt::Vec2(float(swapchainX), float(swapchainY));
	m_frameConstants.screenDimsRcp = kt::Vec2(1.0f / float(swapchainX), 1.0f / float(swapchainY));

	m_frameConstants.numLights = m_lights.Size();

	m_frameConstants.time.x += _dt;
	m_frameConstants.time.y += _dt*0.1f;
	m_frameConstants.time.z = _dt;

	{
	// calculate cascades
		gpu::TextureDesc desc;
		gpu::ResourceType f;
		gpu::GetResourceInfo(m_shadowCascadeTex, f, nullptr, &desc);
		gfx::CalculateShadowCascades(_mainView, m_frameConstants.sunDir, desc.m_width, c_numShadowCascades, m_shadowCascades, &m_frameConstants.cascadeSplits[0]);

		for (uint32_t cascadeIdx = 0; cascadeIdx < c_numShadowCascades; ++cascadeIdx)
		{
			//m_frameConstants.cascadeMatricies[cascadeIdx] = m_shadowCascades[cascadeIdx].GetViewProj();
			m_frameConstants.cascadeMatricies[cascadeIdx] = kt::Mul(gfx::NDC_To_UV_Matrix(), m_shadowCascades[cascadeIdx].GetViewProj());
		}
	}

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

static gpu::TextureHandle GetTextureHandleOrNull(gfx::TextureResHandle _res)
{
	return _res.IsValid() ? res::GetData(_res)->m_gpuTex : gpu::TextureHandle{};
}

void Scene::RenderInstances(gpu::cmd::Context* _ctx, bool _shadowMap)
{
	if (m_instances.Size() == 0)
	{
		return;
	}

	// TODO: Temp allocator here
	kt::Array<InstanceData> instancesSorted;
	// +1 for sentinel
	memcpy(instancesSorted.PushBack_Raw(m_instances.Size() + 1), m_instances.Data(), sizeof(InstanceData) * m_instances.Size());

	{
		// TODO: big if we actually render lots of instance, again - temp allocator.
		InstanceData* tmp = (InstanceData*)KT_ALLOCA(sizeof(InstanceData) * m_instances.Size());
		// -1, don't sort sentinel
		kt::RadixSort(instancesSorted.Begin(), instancesSorted.End() - 1, tmp, [](InstanceData const& _inst) { return _inst.m_modelIdx; });
	}
	instancesSorted.Back().m_modelIdx = UINT32_MAX;

	uint8_t* instanceStream = gpu::cmd::BeginUpdateTransientBuffer(_ctx, m_instanceGpuBuf, sizeof(InstanceData) * m_instances.Size()).Data();

	InstanceData const* begin = instancesSorted.Begin();
	InstanceData const* end = instancesSorted.End() - 1; // -1 for sentinel

	gpu::cmd::SetVertexBuffer(_ctx, _shadowMap ? 1 : 3, m_instanceGpuBuf);
	
	uint32_t batchInstanceBegin = 0;
	for (;;)
	{
		uint32_t const curModelIdx = begin->m_modelIdx;
		KT_ASSERT(curModelIdx < s_models.Size());
		uint32_t numInstances = 0;

		do 
		{
			memcpy(instanceStream, begin->m_data, c_indexDataSize);
			instanceStream += c_indexDataSize;

			++numInstances;
			++begin;
		} while (begin->m_modelIdx == curModelIdx);

		// Write out draw calls
		gfx::Model const& model = *s_models[curModelIdx];
		gpu::cmd::SetVertexBuffer(_ctx, 0, model.m_posGpuBuf);
		gpu::cmd::SetIndexBuffer(_ctx, model.m_indexGpuBuf);

		if (!_shadowMap)
		{
			gpu::cmd::SetVertexBuffer(_ctx, 1, model.m_tangentGpuBuf);
			gpu::cmd::SetVertexBuffer(_ctx, 2, model.m_uv0GpuBuf);
		}

		for (gfx::Model::SubMesh const& mesh : model.m_meshes)
		{
			gfx::Material const& mat = model.m_materials[mesh.m_materialIdx];

			if (!_shadowMap)
			{
				gpu::DescriptorData descriptors[4];
				descriptors[0].Set(GetTextureHandleOrNull(mat.m_albedoTex));
				descriptors[1].Set(GetTextureHandleOrNull(mat.m_normalTex));
				descriptors[2].Set(GetTextureHandleOrNull(mat.m_metallicRoughnessTex));
				descriptors[3].Set(GetTextureHandleOrNull(mat.m_occlusionTex));
				gpu::cmd::SetGraphicsSRVTable(_ctx, descriptors, 0);
			}


			gpu::cmd::DrawIndexedInstanced(_ctx, mesh.m_numIndices, numInstances, mesh.m_indexBufferStartOffset, 0, batchInstanceBegin);
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