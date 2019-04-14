#include <kt/Macros.h>

#include "CommandContext_D3D12.h"
#include "Utils_D3D12.h"
#include "GPUDevice_D3D12.h"

#include <d3d12.h>


#define CHECK_QUEUE_FLAGS(_ctx, _required) \
	KT_MACRO_BLOCK_BEGIN \
		KT_ASSERT(!!(_ctx->m_cmdListFlags & (_required))); \
	KT_MACRO_BLOCK_END

#define CHECK_TRANSIENT_TOUCHED_THIS_FRAME(_ctx, _allocatedBuffer) \
	KT_MACRO_BLOCK_BEGIN \
		KT_ASSERT(!(_allocatedBuffer->m_desc.m_flags & BufferFlags::Transient) || _allocatedBuffer->m_lastFrameTouched == _ctx->m_device->m_frameCounter) \
	KT_MACRO_BLOCK_END

namespace gpu
{

namespace cmd
{


CommandContext_D3D12::CommandContext_D3D12(D3D12_COMMAND_LIST_TYPE _type, Device_D3D12* _dev)
	: m_device(_dev)
	, m_type(_type)
{

	if (_type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
	{
		m_cmdListFlags = CommandListFlags::ComputeQueueFlags;
	}
	else if (_type == D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		m_cmdListFlags = CommandListFlags::DirectQueueFlags;
	}
	else if (_type == D3D12_COMMAND_LIST_TYPE_COPY)
	{
		m_cmdListFlags = CommandListFlags::Copy;
	}

	m_cmdAllocator = _dev->m_commandQueueManager.QueueByType(m_type).AcquireAllocator();
	// TODO: Is it expensive to keep creating command lists (its obviously worth pooling allocators.
	D3D_CHECK(m_device->m_d3dDev->CreateCommandList(1, _type, m_cmdAllocator, nullptr, IID_PPV_ARGS(&m_cmdList)));
}

CommandContext_D3D12::~CommandContext_D3D12()
{
	// Todo: reclaim?
	SafeReleaseDX(m_cmdList);
}

void End(Context* _ctx)
{
	ID3D12CommandList* lists[] = { _ctx->m_cmdList };
	uint64_t const fence = _ctx->m_device->m_commandQueueManager.QueueByType(_ctx->m_type).ExecuteCommandLists(lists, KT_ARRAY_COUNT(lists));
	_ctx->m_device->m_commandQueueManager.QueueByType(_ctx->m_type).ReleaseAllocator(_ctx->m_cmdAllocator, fence);
	_ctx->m_cmdAllocator = nullptr;

	delete _ctx;
}

void SetVertexBuffer(Context* _ctx, uint32_t _streamIdx, gpu::BufferHandle _handle)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags::Graphics);

	KT_ASSERT(_streamIdx < gpu::c_maxVertexStreams);
	if (_ctx->m_state.m_vertexStreams[_streamIdx].Handle() != _handle)
	{
		_ctx->m_state.m_vertexStreams[_streamIdx] = _handle;
		_ctx->m_dirtyFlags |= DirtyStateFlags::VertexBuffer;
	}
}

void SetIndexBuffer(Context* _ctx, gpu::BufferHandle _handle)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags::Graphics);

	if (_ctx->m_state.m_indexBuffer.Handle() != _handle)
	{
		_ctx->m_state.m_indexBuffer = _handle;
		_ctx->m_dirtyFlags |= DirtyStateFlags::IndexBuffer;
	}
}

void DrawIndexedInstanced(Context* _ctx, gpu::PrimitiveType _prim, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startVtx, uint32_t _baseVtx, uint32_t _startInstance)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags::Graphics);

	_ctx->ApplyStateChanges(CommandListFlags::Graphics);

	_ctx->m_cmdList->IASetPrimitiveTopology(ToD3DPrimType(_prim));
	_ctx->m_cmdList->DrawIndexedInstanced(_indexCount, _instanceCount, _startVtx, _baseVtx, _startInstance);
}

void UpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, void const* _mem)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags::Copy);

	KT_ASSERT(_ctx->m_device->m_bufferHandles.IsValid(_handle));
	AllocatedBuffer_D3D12* res = _ctx->m_device->m_bufferHandles.Lookup(_handle);
	KT_ASSERT(!!(res->m_desc.m_flags & BufferFlags::Transient));

	uint32_t const size = res->m_desc.m_sizeInBytes;

	// TODO: Alignment, copying to gpu mem, etc.
	_ctx->m_device->GetFrameResources()->m_uploadAllocator.Alloc(*res, size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	KT_ASSERT(res->m_mappedCpuData);
	memcpy(res->m_mappedCpuData, _mem, size);
	res->m_lastFrameTouched = _ctx->m_device->m_frameCounter;
}

void SetGraphicsPSO(Context* _ctx, gpu::GraphicsPSOHandle _pso)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags::Graphics);

	if (_ctx->m_state.m_graphicsPso.Handle() != _pso)
	{
		_ctx->m_state.m_graphicsPso = _pso;
		_ctx->m_dirtyFlags |= DirtyStateFlags::PipelineState;
	}
}

void ClearRenderTarget(Context* _ctx, gpu::TextureHandle _handle, float const _color[4])
{
	AllocatedTexture_D3D12* tex = _ctx->m_device->m_textureHandles.Lookup(_handle);
	// TODO: Flush barriers for render target? 
	KT_ASSERT(tex);
	KT_ASSERT(tex->m_rtv.ptr);
	KT_ASSERT(tex->m_state == D3D12_RESOURCE_STATE_RENDER_TARGET);
	_ctx->m_cmdList->ClearRenderTargetView(tex->m_rtv, _color, 0, nullptr);
}

void SetRenderTarget(Context* _ctx, uint32_t _idx, gpu::TextureHandle _handle)
{
	if (_ctx->m_state.m_renderTargets[_idx].Handle() != _handle)
	{
#if KT_DEBUG
		if (_handle.IsValid())
		{
			AllocatedTexture_D3D12* tex = _ctx->m_device->m_textureHandles.Lookup(_handle);
			KT_ASSERT(tex);
			KT_ASSERT(tex->m_rtv.ptr);
		}
#endif

		_ctx->m_state.m_renderTargets[_idx] = _handle;
		_ctx->m_dirtyFlags |= DirtyStateFlags::RenderTarget;
	}
}

void SetDepthBuffer(Context* _ctx, gpu::TextureHandle _handle)
{
	if (_ctx->m_state.m_depthBuffer.Handle() != _handle)
	{
#if KT_DEBUG
		if (_handle.IsValid())
		{
			AllocatedTexture_D3D12* tex = _ctx->m_device->m_textureHandles.Lookup(_handle);
			KT_ASSERT(tex);
			KT_ASSERT(tex->m_dsv.ptr);
		}
#endif

		_ctx->m_state.m_depthBuffer = _handle;
		_ctx->m_dirtyFlags |= DirtyStateFlags::DepthBuffer;
	}
}

void ClearDepth(Context* _ctx, gpu::TextureHandle _handle, float _depth)
{
	AllocatedTexture_D3D12* tex = _ctx->m_device->m_textureHandles.Lookup(_handle);
	// TODO: Flush barriers for depth target?
	KT_ASSERT(tex);
	KT_ASSERT(!!(tex->m_desc.m_usageFlags & TextureUsageFlags::DepthStencil));
	KT_ASSERT(tex->m_dsv.ptr);
	_ctx->m_cmdList->ClearDepthStencilView(tex->m_dsv, D3D12_CLEAR_FLAG_DEPTH, _depth, 0, 0, nullptr);
}

void CommandContext_D3D12::ApplyStateChanges(CommandListFlags _dispatchType)
{
	if (!!(_dispatchType & CommandListFlags::Graphics))
	{
		CHECK_QUEUE_FLAGS(this, CommandListFlags::Graphics);

		if (!!(m_dirtyFlags & DirtyStateFlags::PipelineState))
		{
			AllocatedGraphicsPSO_D3D12* pso = m_device->m_psoHandles.Lookup(m_state.m_graphicsPso);
			KT_ASSERT(pso);
			m_state.m_numRenderTargets = pso->m_psoDesc.m_numRenderTargets;
			m_cmdList->SetPipelineState(pso->m_pso);
			m_cmdList->SetGraphicsRootSignature(m_device->m_debugRootSig);
		}

		if (!!(m_dirtyFlags & DirtyStateFlags::IndexBuffer))
		{
			D3D12_INDEX_BUFFER_VIEW idxView = {};
			if (m_state.m_indexBuffer.IsValid())
			{
				AllocatedBuffer_D3D12* idxBuff = m_device->m_bufferHandles.Lookup(m_state.m_indexBuffer);
				CHECK_TRANSIENT_TOUCHED_THIS_FRAME(this, idxBuff);

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

		if (!!(m_dirtyFlags & DirtyStateFlags::VertexBuffer))
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
					CHECK_TRANSIENT_TOUCHED_THIS_FRAME(this, bufferRes);

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

		if (!!(m_dirtyFlags & (DirtyStateFlags::DepthBuffer | DirtyStateFlags::RenderTarget)))
		{
			D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
			D3D12_CPU_DESCRIPTOR_HANDLE rtvs[gpu::c_maxRenderTargets] = {};
			if (m_state.m_depthBuffer.IsValid())
			{
				AllocatedTexture_D3D12* tex = m_device->m_textureHandles.Lookup(m_state.m_depthBuffer);
				KT_ASSERT(tex);
				KT_ASSERT(tex->m_dsv.ptr);
				dsv = tex->m_dsv;
			}

			uint32_t const numRenderTargets = m_state.m_numRenderTargets;
			for (uint32_t i = 0; i < numRenderTargets; ++i)
			{
				KT_ASSERT(m_state.m_renderTargets[i].IsValid());
				AllocatedTexture_D3D12* tex = m_device->m_textureHandles.Lookup(m_state.m_renderTargets[i]);
				KT_ASSERT(tex);
				KT_ASSERT(tex->m_rtv.ptr);
				rtvs[i] = tex->m_rtv;
			}
			m_cmdList->OMSetRenderTargets(numRenderTargets, rtvs, FALSE, &dsv);
		}

	}

	m_dirtyFlags = DirtyStateFlags::None;
}


void SetScissorRect(Context* _ctx, gpu::Rect const& _rect)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags::Graphics);

	D3D12_RECT rect;
	rect.top = _rect.m_topLeft.y;
	rect.left = _rect.m_topLeft.x;
	rect.bottom = _rect.m_bottomRight.y;
	rect.right = _rect.m_bottomRight.x;
	_ctx->m_cmdList->RSSetScissorRects(1, &rect);
}

void SetViewport(Context* _ctx, gpu::Rect const& _rect, float _minDepth, float _maxDepth)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags::Graphics);

	D3D12_VIEWPORT vp;
	vp.TopLeftX = _rect.m_topLeft.x;
	vp.TopLeftY = _rect.m_topLeft.y;

	vp.Height = kt::Abs(_rect.m_topLeft.y - _rect.m_bottomRight.y);
	vp.Width = kt::Abs(_rect.m_bottomRight.x - _rect.m_topLeft.x);
	
	vp.MinDepth = _minDepth;
	vp.MaxDepth = _maxDepth;

	_ctx->m_cmdList->RSSetViewports(1, &vp);
}

}
}