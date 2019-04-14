#include "CommandContext_D3D12.h"
#include "Utils_D3D12.h"
#include "GPUDevice_D3D12.h"

#include <d3d12.h>


namespace gpu
{


CommandContext_D3D12::CommandContext_D3D12(D3D12_COMMAND_LIST_TYPE _type, Device_D3D12* _dev)
	: m_device(_dev)
	, m_type(_type)
{
	m_cmdAllocator = _dev->m_commandQueueManager.QueueByType(m_type).AcquireAllocator();
	// TODO: Is it expensive to keep creating command lists (its obviously worth pooling allocators.
	D3D_CHECK(m_device->m_d3dDev->CreateCommandList(1, _type, m_cmdAllocator, nullptr, IID_PPV_ARGS(&m_cmdList)));
}

CommandContext_D3D12::~CommandContext_D3D12()
{
}

void CommandContext_D3D12::End()
{
	ID3D12CommandList* lists[] = { m_cmdList };
	uint64_t const fence = m_device->m_commandQueueManager.QueueByType(m_type).ExecuteCommandLists(lists, KT_ARRAY_COUNT(lists));
	m_device->m_commandQueueManager.QueueByType(m_type).ReleaseAllocator(m_cmdAllocator, fence);
	m_cmdAllocator = nullptr;
	SafeReleaseDX(m_cmdList);
}

void CommandContext_D3D12::SetVertexBuffer(uint32_t _streamIdx, gpu::BufferHandle _handle)
{
	KT_ASSERT(_streamIdx < gpu::c_maxVertexStreams);
	if (m_state.m_vertexStreams[_streamIdx].Handle() != _handle)
	{
		m_state.m_vertexStreams[_streamIdx] = _handle;
		m_dirtyFlags |= DirtyStateFlags::VertexBuffer;
	}
}

void CommandContext_D3D12::SetIndexBuffer(gpu::BufferHandle _handle)
{
	if (m_state.m_indexBuffer.Handle() != _handle)
	{
		m_state.m_indexBuffer = _handle;
		m_dirtyFlags |= DirtyStateFlags::IndexBuffer;
	}
}

void CommandContext_D3D12::DrawIndexedInstanced(gpu::PrimitiveType _prim, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startVtx, uint32_t _baseVtx, uint32_t _startInstance)
{
	ApplyStateChanges(CommandListFlags::Graphics);

	m_cmdList->IASetPrimitiveTopology(ToD3DPrimType(_prim));
	m_cmdList->DrawIndexedInstanced(_indexCount, _instanceCount, _startVtx, _baseVtx, _startInstance);
}

void CommandContext_D3D12::UpdateTransientBuffer(gpu::BufferHandle _handle, void const* _mem)
{
	KT_ASSERT(m_device->m_bufferHandles.IsValid(_handle));
	AllocatedBuffer_D3D12* res = m_device->m_bufferHandles.Lookup(_handle);
	KT_ASSERT(!!(res->m_desc.m_flags & BufferFlags::Transient));

	uint32_t const size = res->m_desc.m_sizeInBytes;

	// TODO: Alignment, copying to gpu mem, etc.
	m_device->GetFrameResources()->m_uploadAllocator.Alloc(*res, size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	KT_ASSERT(res->m_mappedCpuData);
	memcpy(res->m_mappedCpuData, _mem, size);
	res->m_lastFrameTouched = m_device->m_frameCounter;
}

void CommandContext_D3D12::SetGraphicsPSO(gpu::GraphicsPSOHandle _pso)
{
	if (m_state.m_graphicsPso.Handle() != _pso)
	{
		m_state.m_graphicsPso = _pso;
		m_dirtyFlags |= DirtyStateFlags::PipelineState;
	}
}

void CommandContext_D3D12::ClearRenderTarget(gpu::TextureHandle _handle, float _color[4])
{
	AllocatedTexture_D3D12* tex = m_device->m_textureHandles.Lookup(_handle);
	// TODO: Flush barriers for render target 
	KT_ASSERT(tex);
	KT_ASSERT(tex->m_rtv.ptr);
	KT_ASSERT(tex->m_state == D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_cmdList->ClearRenderTargetView(tex->m_rtv, _color, 0, nullptr);
}

void CommandContext_D3D12::ApplyStateChanges(CommandListFlags _dispatchType)
{
	if (!!(_dispatchType & CommandListFlags::Graphics))
	{
		if (m_dirtyFlags & DirtyStateFlags::PipelineState)
		{
			AllocatedGraphicsPSO_D3D12* pso = m_device->m_psoHandles.Lookup(m_state.m_graphicsPso);
			KT_ASSERT(pso);
			m_cmdList->SetPipelineState(pso->m_pso);
		}

		if (m_dirtyFlags & DirtyStateFlags::IndexBuffer)
		{
			D3D12_INDEX_BUFFER_VIEW idxView = {};
			if (m_state.m_indexBuffer.IsValid())
			{
				AllocatedBuffer_D3D12* idxBuff = m_device->m_bufferHandles.Lookup(m_state.m_indexBuffer);
				KT_ASSERT(idxBuff);
				idxView.BufferLocation = idxBuff->m_gpuAddress;
				idxView.Format = ToDXGIFormat(idxBuff->m_desc.m_format);
				idxView.SizeInBytes = idxBuff->m_desc.m_sizeInBytes;
				m_cmdList->IASetIndexBuffer(&idxView);
			}
			else
			{
				m_cmdList->IASetIndexBuffer(nullptr);
			}
		}

		if (m_dirtyFlags & DirtyStateFlags::VertexBuffer)
		{
			D3D12_VERTEX_BUFFER_VIEW bufferViews[gpu::c_maxVertexStreams] = {};
			uint32_t maxCount = 0;
			for (uint32_t i = 0; i < gpu::c_maxVertexStreams; ++i)
			{
				if (m_state.m_vertexStreams[i].IsValid())
				{
					maxCount = i + 1;

					AllocatedBuffer_D3D12* bufferRes = m_device->m_bufferHandles.Lookup(m_state.m_vertexStreams[i]);
					KT_ASSERT(!!(bufferRes->m_desc.m_flags & BufferFlags::Vertex));

					if (!!(bufferRes->m_desc.m_flags & BufferFlags::Transient))
					{
						KT_ASSERT(bufferRes->m_lastFrameTouched == m_device->m_frameCounter
								  && "Transient resources must be update the frame they are used.");
					}

					bufferViews[i].BufferLocation = bufferRes->m_gpuAddress;
					bufferViews[i].SizeInBytes = bufferRes->m_desc.m_sizeInBytes;
					bufferViews[i].StrideInBytes = bufferRes->m_desc.m_strideInBytes;
				}
			}
			m_cmdList->IASetVertexBuffers(0, maxCount, bufferViews);
		}
	}

	m_dirtyFlags = 0;
}

}