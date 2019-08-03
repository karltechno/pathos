#include <string>
#include <gpu/Types.h>

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
	m_frameConstants.sunDir = kt::Normalize(kt::Vec3(0.4f, -1.0f, 0.15f));

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

static gpu::TextureHandle GetTextureHandleOrNull(ResourceManager::TextureIdx _tex)
{
	return _tex.IsValid() ? ResourceManager::GetTexture(_tex)->m_gpuTex : gpu::TextureHandle{};
}


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

	ResourceManager::MeshIdx m_meshIdx;

	uint8_t __pad0__[sizeof(m_transform) - sizeof(ResourceManager::MeshIdx)];
};

void BuildMeshInstanceArray(kt::Slice<Scene::ModelInstance> const& _modelInstances, kt::Array<InstanceData>& o_instances)
{
	// TODO: Temp allocator, better size.
	o_instances.Reserve(_modelInstances.Size() * 4);

	// TODO: Culling
	// TODO: Slow
	for (Scene::ModelInstance const& modelInstance : _modelInstances)
	{
		gfx::Model const& model = *ResourceManager::GetModel(modelInstance.m_modelIdx);

		float mtx43[4 * 3];
		float const* mtxPtr = modelInstance.m_mtx.Data();
		for (uint32_t col = 0; col < 4; ++col)
		{
			mtx43[col * 3 + 0] = mtxPtr[col * 4 + 0];
			mtx43[col * 3 + 1] = mtxPtr[col * 4 + 1];
			mtx43[col * 3 + 2] = mtxPtr[col * 4 + 2];
		}

		InstanceData* instances = o_instances.PushBack_Raw(model.m_meshes.Size());
		for (ResourceManager::MeshIdx const& meshIdx : model.m_meshes)
		{
			static_assert(sizeof(instances->m_transform) == sizeof(mtx43), "Bad instance data size");
			instances->m_meshIdx = meshIdx;
			memcpy(&instances->m_transform, mtx43, sizeof(mtx43));
			instances->m_transform.m_pos = modelInstance.m_mtx.GetPos();
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
			memcpy(instanceStream, begin->m_data, c_instanceDataSize);
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

		for (gfx::Mesh::SubMesh const& subMesh : mesh.m_subMeshes)
		{
			gfx::Material const& mat = *ResourceManager::GetMaterial(subMesh.m_materialIdx);

			if (!_shadowMap)
			{
				gpu::DescriptorData descriptors[4];
				descriptors[0].Set(GetTextureHandleOrNull(mat.m_textures[gfx::Material::Albedo]));
				descriptors[1].Set(GetTextureHandleOrNull(mat.m_textures[gfx::Material::Normal]));
				descriptors[2].Set(GetTextureHandleOrNull(mat.m_textures[gfx::Material::MetallicRoughness]));
				descriptors[3].Set(GetTextureHandleOrNull(mat.m_textures[gfx::Material::Occlusion]));
				gpu::cmd::SetGraphicsSRVTable(_ctx, descriptors, 0);
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