#include "MeshRenderer.h"

#include <intrin.h>

#include <kt/Sort.h>

#include <core/Memory.h>
#include <shaderlib/DefinesShared.h>

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
	m_indirectArgsBuf.Init(gpu::BufferFlags::Dynamic, 1024, gpu::Format::Unknown, "gfx::Scene indirect args");
	m_instanceUniformsBuf.Init(gpu::BufferFlags::Dynamic | gpu::BufferFlags::ShaderResource, 4096, gpu::Format::Unknown, "gfx::Scene instance uniforms");
	m_instanceIdStepBuf.Init(gpu::BufferFlags::Dynamic | gpu::BufferFlags::Vertex, 4096, gpu::Format::Unknown, "gfx::Scene instance step");
}

void MeshRenderer::Submit(gfx::ResourceManager::MeshIdx _meshIdx, kt::Mat4 const& _mtx)
{
	PackMat44_to_Mat34_Transpose(_mtx, m_transforms3x4.PushBack_Raw());
	m_meshes.PushBack(_meshIdx);
}

void MeshRenderer::BuildMultiDrawBuffers(gpu::cmd::Context* _ctx)
{
	if (m_meshes.Size() == 0)
	{
		m_batchesBuiltThisFrame = 0;
		return;
	}

	uint32_t const maxInstances = m_meshes.Size();

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

	uint32_t* instanceStepRemap = m_instanceIdStepBuf.BeginUpdate(_ctx, maxInstances);
	shaderlib::InstanceData_Xform* xformWrite = m_instanceXformBuf.BeginUpdate(_ctx, maxInstances);

	gpu::cmd::ResourceBarrier(_ctx, m_instanceIdStepBuf.m_buffer, gpu::ResourceState::CopyDest);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceXformBuf.m_buffer, gpu::ResourceState::CopyDest);

	kt::Array<shaderlib::InstanceData_UniformOffsets> uniformOffsets(core::GetThreadFrameAllocator());
	kt::Array<gpu::IndexedDrawArguments> drawArgsData(core::GetThreadFrameAllocator());

	uniformOffsets.Reserve(maxInstances);
	drawArgsData.Reserve(maxInstances);

	uint32_t const* beginInstanceIdx = sortIndices;

	uint32_t numBatches = 0;
	uint32_t globalInstanceIndex = 0;

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
		shaderlib::InstanceData_UniformOffsets* instanceUniforms = uniformOffsets.PushBack_Raw(mesh.m_subMeshes.Size() * numInstancesForThisBatch);

		uint32_t const transformIdxBegin = globalInstanceIndex;

		for (gfx::Mesh::SubMesh const& subMesh : mesh.m_subMeshes)
		{
			drawArgs->m_baseVertex = 0; // This is completely useless with manual vertex fetch, because SV_VertexID does not take it into account.
			drawArgs->m_indexStart = subMesh.m_indexBufferStartOffset + mesh.m_unifiedBufferIndexOffset;
			drawArgs->m_indicesPerInstance = subMesh.m_numIndices;
			drawArgs->m_instanceCount = numInstancesForThisBatch;
			drawArgs->m_startInstance = globalInstanceIndex;
			++drawArgs;

			uint32_t const materialIdx = subMesh.m_materialIdx.idx;
			for (uint32_t i = 0; i < numInstancesForThisBatch; ++i)
			{
				*instanceStepRemap++ = globalInstanceIndex++;
				instanceUniforms->transformIdx = transformIdxBegin + i;
				instanceUniforms->materialIdx = materialIdx;
				instanceUniforms->baseVtx = mesh.m_unifiedBufferVertexOffset;
				++instanceUniforms;
			}
		}

		// Check sentinel.
		if (!nextMeshIdx.IsValid())
		{
			break;
		}
	}

	gpu::cmd::ResourceBarrier(_ctx, m_indirectArgsBuf.m_buffer, gpu::ResourceState::CopyDest);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceUniformsBuf.m_buffer, gpu::ResourceState::CopyDest);

	gpu::cmd::FlushBarriers(_ctx);
	m_indirectArgsBuf.Update(_ctx, drawArgsData.Data(), drawArgsData.Size());
	m_instanceUniformsBuf.Update(_ctx, uniformOffsets.Data(), uniformOffsets.Size());
	m_instanceXformBuf.EndUpdate(_ctx);
	m_instanceIdStepBuf.EndUpdate(_ctx);

	gpu::cmd::ResourceBarrier(_ctx, m_indirectArgsBuf.m_buffer, gpu::ResourceState::IndirectArg);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceUniformsBuf.m_buffer, gpu::ResourceState::ShaderResource);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceXformBuf.m_buffer, gpu::ResourceState::ShaderResource);
	gpu::cmd::ResourceBarrier(_ctx, m_instanceIdStepBuf.m_buffer, gpu::ResourceState::VertexBuffer);

	m_batchesBuiltThisFrame = numBatches;
}

void MeshRenderer::RenderInstances(gpu::cmd::Context* _ctx)
{
	if (!m_batchesBuiltThisFrame)
	{
		return;
	}

	gpu::cmd::SetVertexBuffer(_ctx, 0, m_instanceIdStepBuf.m_buffer);

	gpu::cmd::SetIndexBuffer(_ctx, gfx::ResourceManager::GetUnifiedBuffers().m_indexBufferRef);

	gpu::DescriptorData viewDescriptors[2];
	viewDescriptors[0].Set(m_instanceXformBuf.m_buffer);
	viewDescriptors[1].Set(m_instanceUniformsBuf.m_buffer);
	gpu::cmd::SetGraphicsSRVTable(_ctx, viewDescriptors, PATHOS_PER_VIEW_SPACE);

	gpu::cmd::DrawIndexedInstancedIndirect(_ctx, m_indirectArgsBuf.m_buffer, 0, m_batchesBuiltThisFrame);
}


void MeshRenderer::Clear()
{
	m_transforms3x4.Clear();
	m_meshes.Clear();

	m_batchesBuiltThisFrame = 0;
}

}