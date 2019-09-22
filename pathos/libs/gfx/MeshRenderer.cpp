#include "MeshRenderer.h"

#include <intrin.h>

#include <kt/Sort.h>

#include <core/Memory.h>
#include <shaderlib/DefinesShared.h>
#include <shaderlib/CullingShared.h>

#include "Model.h"

namespace gfx
{

static void PackMat44_to_Mat34_Transpose(kt::Mat4 const& _mat4, MeshRenderer::Matrix3x4* o_mat34)
{
	float const* mtxPtr = _mat4.Data();

	for (uint32_t row = 0; row < 3; ++row)
	{
		o_mat34->data[row * 4 + 0] = mtxPtr[row + 0];
		o_mat34->data[row * 4 + 1] = mtxPtr[row + 4];
		o_mat34->data[row * 4 + 2] = mtxPtr[row + 8];
		o_mat34->data[row * 4 + 3] = mtxPtr[row + 12];
	}
}

MeshRenderer::MeshRenderer()
{
	m_instanceXformBuf.Init(gpu::BufferFlags::Dynamic | gpu::BufferFlags::ShaderResource, 4096, gpu::Format::Unknown, "gfx::Scene instance xforms");
	m_indirectArgsBuf.Init(gpu::BufferFlags::Dynamic | gpu::BufferFlags::UnorderedAccess, 1024, gpu::Format::Unknown, "gfx::Scene indirect args");
	m_instanceIdx_MeshIdx_Buf.Init(gpu::BufferFlags::Dynamic | gpu::BufferFlags::Vertex | gpu::BufferFlags::UnorderedAccess, 4096, gpu::Format::Unknown, "gfx::Scene instanceIdx_meshIdx");
}

void MeshRenderer::Submit(gfx::ResourceManager::MeshIdx _meshIdx, kt::Mat4 const& _mtx)
{
	PackMat44_to_Mat34_Transpose(_mtx, m_transforms3x4.PushBack_Raw());
	m_numSubmeshesSubmittedThisFrame += gfx::ResourceManager::GetMesh(_meshIdx)->m_subMeshes.Size();
	m_meshes.PushBack(_meshIdx);
}

void MeshRenderer::BuildMultiDrawBuffersCPU(gpu::cmd::Context* _ctx)
{
	if (m_meshes.Size() == 0)
	{
		m_batchesBuiltThisFrame = 0;
		return;
	}

	GPU_PROFILE_SCOPE(_ctx, "MeshRenderer::BuildMultiDrawBuffersCPU", GPU_PROFILE_COLOUR(0x00, 0x00, 0xff));

	uint32_t const numMeshInstances = m_meshes.Size();

	// add end of buffer sentinel.
	m_meshes.PushBack(gfx::ResourceManager::MeshIdx{});

	uint32_t* sortIndices = (uint32_t*)core::GetThreadFrameAllocator()->Alloc(sizeof(uint32_t) * m_meshes.Size());

	for (uint32_t i = 0; i < m_meshes.Size(); ++i)
	{
		sortIndices[i] = i;
	}

	{
		// Sort by just mesh for now.
		uint32_t* sortIndicesTemp = (uint32_t*)core::GetThreadFrameAllocator()->Alloc(sizeof(uint32_t) * m_meshes.Size());
		kt::RadixSort(sortIndices, sortIndices + m_meshes.Size() - 1, sortIndicesTemp, [this](uint32_t _v) -> uint16_t { return m_meshes[_v].idx; });
	}

	gpu::cmd::ResourceBarrier(_ctx, m_instanceIdx_MeshIdx_Buf.m_buffer, gpu::ResourceState::CopyDest);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceXformBuf.m_buffer, gpu::ResourceState::CopyDest);

	uint32_t* instanceIdx_meshIdxWrite = m_instanceIdx_MeshIdx_Buf.BeginUpdate(_ctx, m_numSubmeshesSubmittedThisFrame);
	shaderlib::InstanceData_Xform* xformWrite = m_instanceXformBuf.BeginUpdate(_ctx, numMeshInstances);

	kt::Array<gpu::IndexedDrawArguments> drawArgsData(core::GetThreadFrameAllocator());

	drawArgsData.Reserve(numMeshInstances);

	uint32_t const* beginInstanceIdx = sortIndices;

	uint32_t numBatches = 0;
	uint32_t globalInstanceIndex = 0;
	uint32_t globalTransformIdx = 0;

	for (;;)
	{
 		ResourceManager::MeshIdx const curMeshIdx = m_meshes[*beginInstanceIdx];

		ResourceManager::MeshIdx nextMeshIdx;
		uint32_t numInstancesForThisBatch = 0;

		do
		{
			Matrix3x4 const& mtx34 = m_transforms3x4[*beginInstanceIdx];
			
			_mm_store_ps((float*)&xformWrite->row0, _mm_load_ps(mtx34.data));
			_mm_store_ps((float*)&xformWrite->row1, _mm_load_ps(mtx34.data + 4));
			_mm_store_ps((float*)&xformWrite->row2, _mm_load_ps(mtx34.data + 8));
			++xformWrite;
			++numInstancesForThisBatch;
			++beginInstanceIdx;

			nextMeshIdx = m_meshes[*beginInstanceIdx];

		} while (nextMeshIdx == curMeshIdx);

		gfx::Mesh const& mesh = *ResourceManager::GetMesh(curMeshIdx);

		numBatches += mesh.m_subMeshes.Size();
		gpu::IndexedDrawArguments* drawArgs = drawArgsData.PushBack_Raw(mesh.m_subMeshes.Size());

		uint32_t subMeshGpuOffset = mesh.m_gpuSubMeshDataOffset;

		for (gfx::Mesh::SubMesh const& subMesh : mesh.m_subMeshes)
		{
			uint32_t transformIdxBegin = globalTransformIdx;
			KT_ASSERT((transformIdxBegin + numInstancesForThisBatch) <= (1 << PATHOS_INSTANCE_ID_REMAP_BITS)); // if we hit this, change bit allocations or break into batches.
			KT_ASSERT((subMeshGpuOffset + mesh.m_subMeshes.Size()) <= (1 << PATHOS_SUBMESH_ID_REMAP_BITS));
			
			drawArgs->m_baseVertex = 0; // This is completely useless with manual vertex fetch, because SV_VertexID does not take it into account.
			drawArgs->m_indexStart = subMesh.m_indexBufferStartOffset + mesh.m_unifiedBufferIndexOffset;
			drawArgs->m_indicesPerInstance = subMesh.m_numIndices;
			drawArgs->m_instanceCount = numInstancesForThisBatch;
			drawArgs->m_startInstance = globalInstanceIndex;
			++drawArgs;

			uint32_t instancesToWrite = numInstancesForThisBatch;

			do
			{
				*instanceIdx_meshIdxWrite++ = (transformIdxBegin++) | (subMeshGpuOffset << PATHOS_SUBMESH_ID_REMAP_SHIFT);
			} while (--instancesToWrite);

			globalInstanceIndex += numInstancesForThisBatch;
			++subMeshGpuOffset;
		}

		globalTransformIdx += numInstancesForThisBatch;

		// Check sentinel.
		if (!nextMeshIdx.IsValid())
		{
			break;
		}
	}

	gpu::cmd::ResourceBarrier(_ctx, m_indirectArgsBuf.m_buffer, gpu::ResourceState::CopyDest);

	gpu::cmd::FlushBarriers(_ctx);
	m_indirectArgsBuf.Update(_ctx, drawArgsData.Data(), drawArgsData.Size());
	m_instanceXformBuf.EndUpdate(_ctx);
	m_instanceIdx_MeshIdx_Buf.EndUpdate(_ctx);

	gpu::cmd::ResourceBarrier(_ctx, m_indirectArgsBuf.m_buffer, gpu::ResourceState::IndirectArg);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceXformBuf.m_buffer, gpu::ResourceState::ShaderResource);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceIdx_MeshIdx_Buf.m_buffer, gpu::ResourceState::VertexBuffer);

	m_batchesBuiltThisFrame = numBatches;
}

void MeshRenderer::BuildMultiDrawBuffersGPU(gpu::cmd::Context* _ctx, GPUCullingBuffers& _scratchCullBuffers)
{
	if (m_meshes.Size() == 0)
	{
		m_batchesBuiltThisFrame = 0;
		return;
	}

	GPU_PROFILE_SCOPE(_ctx, "MeshRenderer::BuildMultiDrawBuffersGPU", GPU_PROFILE_COLOUR(0x00, 0x00, 0xff));

	uint32_t const numMeshInstances = m_meshes.Size();

	gpu::cmd::ResourceBarrier(_ctx, m_instanceXformBuf.m_buffer, gpu::ResourceState::CopyDest);
	gpu::cmd::ResourceBarrier(_ctx, _scratchCullBuffers.instanceCullingData.m_buffer, gpu::ResourceState::CopyDest);

	shaderlib::InstanceData_Xform* xformWrite = m_instanceXformBuf.BeginUpdate(_ctx, numMeshInstances);
	uint32_t* cullingDataWrite = _scratchCullBuffers.instanceCullingData.BeginUpdate(_ctx, m_numSubmeshesSubmittedThisFrame);


	for (uint32_t transformIdx = 0; transformIdx < m_meshes.Size(); ++transformIdx)
	{
		gfx::Mesh const* mesh = gfx::ResourceManager::GetMesh(m_meshes[transformIdx]);
		Matrix3x4 const& mtx = m_transforms3x4[transformIdx];

		_mm_store_ps((float*)&xformWrite->row0, _mm_load_ps(mtx.data));
		_mm_store_ps((float*)&xformWrite->row1, _mm_load_ps(mtx.data + 4));
		_mm_store_ps((float*)&xformWrite->row2, _mm_load_ps(mtx.data + 8));
		++xformWrite;

		uint32_t submeshIdx = mesh->m_gpuSubMeshDataOffset;

		for (uint32_t j = 0; j < mesh->m_subMeshes.Size(); ++j)
		{
			*cullingDataWrite++ = transformIdx | (submeshIdx++ << PATHOS_SUBMESH_ID_REMAP_SHIFT);
		}
	}

	gpu::cmd::FlushBarriers(_ctx);
	m_instanceXformBuf.EndUpdate(_ctx);
	_scratchCullBuffers.instanceCullingData.EndUpdate(_ctx);

	gpu::cmd::ResourceBarrier(_ctx, m_instanceXformBuf.m_buffer, gpu::ResourceState::ShaderResource);
	gpu::cmd::ResourceBarrier(_ctx, _scratchCullBuffers.instanceCullingData.m_buffer, gpu::ResourceState::ShaderResource);

	// Both of these are filled by gpu.
	m_instanceIdx_MeshIdx_Buf.EnsureSize(_ctx, m_numSubmeshesSubmittedThisFrame, false);
	m_indirectArgsBuf.EnsureSize(_ctx, m_numSubmeshesSubmittedThisFrame + 1, false); // +1 for counter

	gpu::cmd::ResourceBarrier(_ctx, m_instanceIdx_MeshIdx_Buf.m_buffer, gpu::ResourceState::UnorderedAccess);
	gpu::cmd::ResourceBarrier(_ctx, m_indirectArgsBuf.m_buffer, gpu::ResourceState::UnorderedAccess);

	gpu::DescriptorData srvs[3];
	gpu::DescriptorData uavs[2];
	gpu::DescriptorData cbvs[1];

	srvs[0].Set(_scratchCullBuffers.instanceCullingData.m_buffer);
	srvs[1].Set(m_instanceXformBuf.m_buffer);
	srvs[2].Set(gfx::ResourceManager::GetUnifiedBuffers().m_submeshGpuBuf.m_buffer);

	uavs[0].Set(m_indirectArgsBuf.m_buffer);
	uavs[1].Set(m_instanceIdx_MeshIdx_Buf.m_buffer);

	shaderlib::CullingConstants constants;
	constants.numSubmeshInstances = m_numSubmeshesSubmittedThisFrame;
	cbvs[0].Set(&constants, sizeof(constants));

	gpu::cmd::SetComputeCBVTable(_ctx, cbvs, PATHOS_PER_BATCH_SPACE);
	gpu::cmd::SetComputeSRVTable(_ctx, srvs, PATHOS_PER_BATCH_SPACE);
	gpu::cmd::SetComputeUAVTable(_ctx, uavs, PATHOS_PER_BATCH_SPACE);

	// Clear arg count
	{
		gpu::cmd::SetPSO(_ctx, gfx::ResourceManager::GetSharedResources().m_clearDrawCountPso);
		gpu::cmd::Dispatch(_ctx, 1, 1, 1);
		gpu::cmd::UAVBarrier(_ctx, m_indirectArgsBuf.m_buffer);
	}

	{
		gpu::cmd::SetPSO(_ctx, gfx::ResourceManager::GetSharedResources().m_cullSubmeshPso);
		gpu::cmd::Dispatch(_ctx, (m_numSubmeshesSubmittedThisFrame + 63) / 64, 1, 1);
	}

	gpu::cmd::ResourceBarrier(_ctx, m_indirectArgsBuf.m_buffer, gpu::ResourceState::IndirectArg);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceIdx_MeshIdx_Buf.m_buffer, gpu::ResourceState::ShaderResource);

	m_builtThisFrameOnGPU = true;
	m_batchesBuiltThisFrame = m_numSubmeshesSubmittedThisFrame;
}


void MeshRenderer::RenderInstances(gpu::cmd::Context* _ctx)
{
	if (!m_batchesBuiltThisFrame)
	{
		return;
	}

	GPU_PROFILE_SCOPE(_ctx, "MeshRenderer::RenderInstances", GPU_PROFILE_COLOUR(0x00, 0xff, 0xff));

	gpu::cmd::SetVertexBuffer(_ctx, 0, m_instanceIdx_MeshIdx_Buf.m_buffer);

	gpu::cmd::SetIndexBuffer(_ctx, gfx::ResourceManager::GetUnifiedBuffers().m_indexBufferRef);

	gpu::DescriptorData viewDescriptors[1];
	viewDescriptors[0].Set(m_instanceXformBuf.m_buffer);
	gpu::cmd::SetGraphicsSRVTable(_ctx, viewDescriptors, PATHOS_PER_VIEW_SPACE);

	if (m_builtThisFrameOnGPU)
	{
		gpu::cmd::DrawIndexedInstancedIndirect(_ctx, m_indirectArgsBuf.m_buffer, sizeof(uint32_t), m_batchesBuiltThisFrame, m_indirectArgsBuf.m_buffer, 0);
	}
	else
	{
		gpu::cmd::DrawIndexedInstancedIndirect(_ctx, m_indirectArgsBuf.m_buffer, 0, m_batchesBuiltThisFrame);
	}
}


void MeshRenderer::Clear()
{
	m_transforms3x4.Clear();
	m_meshes.Clear();

	m_batchesBuiltThisFrame = 0;
	m_numSubmeshesSubmittedThisFrame = 0;
	m_builtThisFrameOnGPU = false;
}

}