#include <kt/Macros.h>

#include "CommandContext_D3D12.h"
#include "Utils_D3D12.h"
#include "GPUDevice_D3D12.h"
#include "DescriptorHeap_D3D12.h"

#include <d3d12.h>


#define CHECK_QUEUE_FLAGS(_ctx, _required) \
	KT_MACRO_BLOCK_BEGIN \
		KT_ASSERT(!!(_ctx->m_cmdListFlags & (_required))); \
	KT_MACRO_BLOCK_END

#define CHECK_TRANSIENT_TOUCHED_THIS_FRAME(_ctx, _allocatedBuffer) \
	KT_MACRO_BLOCK_BEGIN \
		KT_ASSERT(!(_allocatedBuffer->m_desc.m_flags & BufferFlags::Transient) || _allocatedBuffer->m_lastFrameTouched == _ctx->m_device->m_frameCounter); \
	KT_MACRO_BLOCK_END

namespace gpu
{

namespace cmd
{


Context* Begin(ContextType _type)
{
	return new CommandContext_D3D12(_type, g_device);
}

ContextType GetContextType(Context* _ctx)
{
	return _ctx->m_ctxType;
}

CommandContext_D3D12::CommandContext_D3D12(ContextType _type, Device_D3D12* _dev)
	: m_device(_dev)
	, m_ctxType(_type)
{

	switch (_type)
	{
		case ContextType::Compute:
		{
			m_d3dType = D3D12_COMMAND_LIST_TYPE_COMPUTE;
			m_cmdListFlags = CommandListFlags_D3D12::ComputeQueueFlags;

		} break;

		case ContextType::Graphics:
		{
			m_d3dType = D3D12_COMMAND_LIST_TYPE_DIRECT;
			m_cmdListFlags = CommandListFlags_D3D12::DirectQueueFlags;
		} break;

		case ContextType::Copy:
		{
			m_d3dType = D3D12_COMMAND_LIST_TYPE_COPY;
			m_cmdListFlags = CommandListFlags_D3D12::CopyQueueFlags;
		} break;

		default:
		{
			KT_ASSERT(false);
			KT_UNREACHABLE;
		}
	}


	m_cmdAllocator = _dev->m_commandQueueManager.QueueByType(m_d3dType).AcquireAllocator();
	// TODO: Is it expensive to keep creating command lists (its obviously worth pooling allocators).
	D3D_CHECK(m_device->m_d3dDev->CreateCommandList(1, m_d3dType, m_cmdAllocator, nullptr, IID_PPV_ARGS(&m_cmdList)));

	for (uint32_t i = 0; i < KT_ARRAY_COUNT(m_dirtyDescriptors); ++i)
	{
		m_dirtyDescriptors[i] = DirtyDescriptorFlags::All;
	}

	uint32_t swapchainHeight, swapchainWidth;
	gpu::GetSwapchainDimensions(swapchainWidth, swapchainHeight);
	m_state.m_viewport.m_rect = gpu::Rect(float(swapchainWidth), float(swapchainHeight));
	
	// TODO: reverse Z
	m_state.m_viewport.m_depthMin = 0.0f;
	m_state.m_viewport.m_depthMax = 1.0f;

	m_state.m_scissorRect = m_state.m_viewport.m_rect;

	m_dirtyFlags |= DirtyStateFlags::ViewPort | DirtyStateFlags::ScissorRect;

	ID3D12DescriptorHeap* heap = m_device->m_cbvsrvuavHeap.m_heap;
	m_cmdList->SetDescriptorHeaps(1, &heap);

	if (m_d3dType == D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		// TODO: HACK
		m_cmdList->SetGraphicsRootSignature(m_device->m_graphicsRootSig);
	}
}

CommandContext_D3D12::~CommandContext_D3D12()
{
	// Todo: reclaim?
	SafeReleaseDX(m_cmdList);
}

void End(Context* _ctx)
{
	ID3D12CommandList* list = _ctx->m_cmdList;

	uint64_t const fence = _ctx->m_device->m_commandQueueManager.QueueByType(_ctx->m_d3dType).ExecuteCommandLists(kt::MakeSlice(list));
	_ctx->m_device->m_commandQueueManager.QueueByType(_ctx->m_d3dType).ReleaseAllocator(_ctx->m_cmdAllocator, fence);
	_ctx->m_cmdAllocator = nullptr;

	delete _ctx;
}

void SetVertexBuffer(Context* _ctx, uint32_t _streamIdx, gpu::BufferHandle _handle)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	KT_ASSERT(_streamIdx < gpu::c_maxVertexStreams);
	if (_ctx->m_state.m_vertexStreams[_streamIdx].Handle() != _handle)
	{
		_ctx->m_state.m_vertexStreams[_streamIdx].Acquire(_handle);
		_ctx->m_dirtyFlags |= DirtyStateFlags::VertexBuffer;
	}
}

void SetIndexBuffer(Context* _ctx, gpu::BufferHandle _handle)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	if (_ctx->m_state.m_indexBuffer.Handle() != _handle)
	{
		_ctx->m_state.m_indexBuffer.Acquire(_handle);
		_ctx->m_dirtyFlags |= DirtyStateFlags::IndexBuffer;
	}
}

void SetShaderResource(Context* _ctx, gpu::TextureHandle _handle, uint32_t _idx, uint32_t _space)
{
	if (_ctx->m_state.m_srvTex[_space][_idx].Handle() != _handle)
	{
		_ctx->m_state.m_srvTex[_space][_idx].Acquire(_handle);
		_ctx->m_dirtyDescriptors[_space] |= DirtyDescriptorFlags::SRV;
	}
}


void DrawIndexedInstanced(Context* _ctx, gpu::PrimitiveType _prim, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startVtx, uint32_t _baseVtx, uint32_t _startInstance)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	_ctx->ApplyStateChanges(CommandListFlags_D3D12::Graphics);

	_ctx->m_cmdList->IASetPrimitiveTopology(ToD3DPrimType(_prim));
	
	//float blend[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	//_ctx->m_cmdList->OMSetBlendFactor(blend);
	_ctx->m_cmdList->DrawIndexedInstanced(_indexCount, _instanceCount, _startVtx, _baseVtx, _startInstance);
}

void UpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, void const* _mem, uint32_t _newSize /* = 0xFFFFFFFF */)
{
	kt::Slice<uint8_t> slice = BeginUpdateTransientBuffer(_ctx, _handle, _newSize);
	memcpy(slice.Data(), _mem, slice.Size());
	EndUpdateTransientBuffer(_ctx, _handle);
}

kt::Slice<uint8_t> BeginUpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, uint32_t _newSize /*= 0xFFFFFFFF */)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Copy);

	KT_ASSERT(_ctx->m_device->m_bufferHandles.IsValid(_handle));
	AllocatedBuffer_D3D12* res = _ctx->m_device->m_bufferHandles.Lookup(_handle);
	KT_ASSERT(!!(res->m_desc.m_flags & BufferFlags::Transient));

	if (_newSize != 0xFFFFFFFF)
	{
		res->m_desc.m_sizeInBytes = _newSize;
	}

	_ctx->m_device->GetFrameResources()->m_uploadAllocator.Alloc(*res);

	KT_ASSERT(res->m_mappedCpuData);
	res->m_lastFrameTouched = _ctx->m_device->m_frameCounter;
	res->UpdateViews();
	_ctx->MarkDirtyIfBound(_handle);
	return kt::MakeSlice((uint8_t*)res->m_mappedCpuData, res->m_desc.m_sizeInBytes);
}

void EndUpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle)
{
	KT_UNUSED2(_ctx, _handle);
}

void SetGraphicsPSO(Context* _ctx, gpu::GraphicsPSOHandle _pso)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	if (_ctx->m_state.m_graphicsPso.Handle() != _pso)
	{
		_ctx->m_state.m_graphicsPso.Acquire(_pso);
		_ctx->m_dirtyFlags |= DirtyStateFlags::PipelineState;
	}
}

void ClearRenderTarget(Context* _ctx, gpu::TextureHandle _handle, float const _color[4])
{
	AllocatedTexture_D3D12* tex = _ctx->m_device->m_textureHandles.Lookup(_handle);
	// TODO: Flush barriers for render target
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

		_ctx->m_state.m_renderTargets[_idx].Acquire(_handle);
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

		_ctx->m_state.m_depthBuffer.Acquire(_handle);
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

D3D12_GPU_DESCRIPTOR_HANDLE UpdateDescriptorTable(CommandContext_D3D12* _ctx, kt::Slice<D3D12_CPU_DESCRIPTOR_HANDLE> const& _srcHandles)
{
	UINT const tableSize = _srcHandles.Size();
	UINT* sourceSizes = (UINT*)KT_ALLOCA(tableSize * sizeof(UINT));
	for (uint32_t i = 0; i < tableSize; ++i)
	{
		sourceSizes[i] = 1;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE tableDestGpu;
	D3D12_CPU_DESCRIPTOR_HANDLE tableDestCpu;
	UINT const destSize = gpu::c_cbvTableSize;
	_ctx->m_device->m_descriptorcbvsrvuavRingBuffer.Alloc(tableSize, tableDestCpu, tableDestGpu);
	_ctx->m_device->m_d3dDev->CopyDescriptors(1, &tableDestCpu, &destSize, tableSize, _srcHandles.Data(), sourceSizes, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return tableDestGpu;
}

void CommandContext_D3D12::ApplyStateChanges(CommandListFlags_D3D12 _dispatchType)
{
	if (!!(_dispatchType & CommandListFlags_D3D12::Graphics))
	{
		CHECK_QUEUE_FLAGS(this, CommandListFlags_D3D12::Graphics);

		if (!!(m_dirtyFlags & DirtyStateFlags::ViewPort))
		{
			D3D12_VIEWPORT vp{};
			vp.MinDepth = m_state.m_viewport.m_depthMin;
			vp.MaxDepth = m_state.m_viewport.m_depthMax;
			vp.TopLeftX = m_state.m_viewport.m_rect.m_topLeft.x;
			vp.TopLeftY = m_state.m_viewport.m_rect.m_topLeft.y;
			vp.Height = kt::Abs(m_state.m_viewport.m_rect.m_topLeft.y - m_state.m_viewport.m_rect.m_bottomRight.y);
			vp.Width = kt::Abs(m_state.m_viewport.m_rect.m_bottomRight.x - m_state.m_viewport.m_rect.m_topLeft.x);

			m_cmdList->RSSetViewports(1, &vp);
		}

		if (!!(m_dirtyFlags & DirtyStateFlags::ScissorRect))
		{
			D3D12_RECT rect;
			rect.bottom = m_state.m_scissorRect.m_bottomRight.y;
			rect.right = m_state.m_scissorRect.m_bottomRight.x;
			rect.top = m_state.m_scissorRect.m_topLeft.y;
			rect.left = m_state.m_scissorRect.m_topLeft.x;
			m_cmdList->RSSetScissorRects(1, &rect);
		}

		if (!!(m_dirtyFlags & DirtyStateFlags::PipelineState))
		{
			AllocatedGraphicsPSO_D3D12* pso = m_device->m_psoHandles.Lookup(m_state.m_graphicsPso);
			KT_ASSERT(pso);
			m_state.m_numRenderTargets = pso->m_psoDesc.m_numRenderTargets;
			m_cmdList->SetPipelineState(pso->m_pso);
		}

		if (!!(m_dirtyFlags & DirtyStateFlags::IndexBuffer))
		{
			D3D12_INDEX_BUFFER_VIEW idxView = {};
			if (m_state.m_indexBuffer.IsValid())
			{
				AllocatedBuffer_D3D12* idxBuff = m_device->m_bufferHandles.Lookup(m_state.m_indexBuffer);
				KT_ASSERT(idxBuff);
				CHECK_TRANSIENT_TOUCHED_THIS_FRAME(this, idxBuff);

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
					KT_ASSERT(bufferRes);
					KT_ASSERT(!!(bufferRes->m_desc.m_flags & BufferFlags::Vertex));
					CHECK_TRANSIENT_TOUCHED_THIS_FRAME(this, bufferRes);

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
			m_cmdList->OMSetRenderTargets(numRenderTargets, rtvs, FALSE, dsv.ptr ? &dsv : nullptr);
		}
	}

	if (!!(_dispatchType & (CommandListFlags_D3D12::Compute | CommandListFlags_D3D12::Graphics)))
	{
		for (uint32_t spaceIdx = 0; spaceIdx < gpu::c_numShaderSpaces; ++spaceIdx)
		{
			DirtyDescriptorFlags const dirtyDescriptors = m_dirtyDescriptors[spaceIdx];
			if (dirtyDescriptors == DirtyDescriptorFlags::None)
			{
				continue;
			}

			if (!!(dirtyDescriptors & DirtyDescriptorFlags::CBV))
			{
				D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptors[gpu::c_cbvTableSize];
				for (uint32_t i = 0; i < gpu::c_cbvTableSize; ++i)
				{
					gpu::BufferRef const& bufRef = m_state.m_cbvs[spaceIdx][i];
					if (!bufRef.IsValid())
					{
						cpuDescriptors[i] = m_device->m_nullCbv;
					}
					else
					{
						AllocatedBuffer_D3D12* buf = m_device->m_bufferHandles.Lookup(bufRef);
						CHECK_TRANSIENT_TOUCHED_THIS_FRAME(this, buf);
						KT_ASSERT(buf);
						KT_ASSERT(buf->m_cbv.ptr);
						cpuDescriptors[i] = buf->m_cbv;
					}
				}
				D3D12_GPU_DESCRIPTOR_HANDLE const gpuDest = UpdateDescriptorTable(this, kt::MakeSlice(cpuDescriptors));
				// TODO: Hardcoded root offset.
				if (!!(_dispatchType & CommandListFlags_D3D12::Graphics))
				{
					m_cmdList->SetGraphicsRootDescriptorTable(0 + 3 * spaceIdx, gpuDest);
				}
				else
				{
					m_cmdList->SetComputeRootDescriptorTable(0 + 3 * spaceIdx, gpuDest);
				}
			}

			if (!!(dirtyDescriptors & DirtyDescriptorFlags::SRV))
			{
				D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptors[gpu::c_srvTableSize];
				for (uint32_t i = 0; i < gpu::c_srvTableSize; ++i)
				{
					gpu::TextureRef const& texRef = m_state.m_srvTex[spaceIdx][i];
					if (!texRef.IsValid())
					{
						cpuDescriptors[i] = m_device->m_nullCbv;
					}
					else
					{
						AllocatedTexture_D3D12* texData = m_device->m_textureHandles.Lookup(texRef);
						KT_ASSERT(texData);
						KT_ASSERT(texData->m_srv.ptr);
						cpuDescriptors[i] = texData->m_srv;
					}
				}

				D3D12_GPU_DESCRIPTOR_HANDLE const gpuDest = UpdateDescriptorTable(this, kt::MakeSlice(cpuDescriptors));
				// TODO: Hardcoded root offset.
				if (!!(_dispatchType & CommandListFlags_D3D12::Graphics))
				{
					m_cmdList->SetGraphicsRootDescriptorTable(1 + 3 * spaceIdx, gpuDest);
				}
				else
				{
					m_cmdList->SetComputeRootDescriptorTable(1 + 3 * spaceIdx, gpuDest);
				}
			}

			m_dirtyDescriptors[spaceIdx] = DirtyDescriptorFlags::None;
		}
	}

	// TODO: BROKEN FOR COMPUTE (clear state flags we haven't updated!)
	m_dirtyFlags = DirtyStateFlags::None;
}


void CommandContext_D3D12::MarkDirtyIfBound(gpu::BufferHandle _handle)
{
	for (gpu::BufferRef const& buff : m_state.m_vertexStreams)
	{
		if (buff.Handle() == _handle)
		{
			m_dirtyFlags |= DirtyStateFlags::VertexBuffer;
			break;
		}
	}

	if (m_state.m_indexBuffer.Handle() == _handle)
	{
		m_dirtyFlags |= DirtyStateFlags::IndexBuffer;
	}
	
	for (uint32_t space = 0; space < gpu::c_numShaderSpaces; ++space)
	{
		for (gpu::BufferRef const& buff : m_state.m_cbvs[space])
		{
			if (buff.Handle() == _handle)
			{
				m_dirtyDescriptors[space] |= DirtyDescriptorFlags::CBV;
				break;
			}
		}
	}
}

void SetConstantBuffer(Context* _ctx, gpu::BufferHandle _handle, uint32_t _idx, uint32_t _space)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Compute);
	if (_ctx->m_state.m_cbvs[_space][_idx].Handle() != _handle)
	{
		_ctx->m_state.m_cbvs[_space][_idx].Acquire(_handle);
		_ctx->m_dirtyDescriptors[_space] |= DirtyDescriptorFlags::CBV;
	}
}

void SetScissorRect(Context* _ctx, gpu::Rect const& _rect)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);
	
	if (_rect.m_bottomRight != _ctx->m_state.m_scissorRect.m_bottomRight
		|| _rect.m_topLeft != _ctx->m_state.m_scissorRect.m_topLeft)
	{
		_ctx->m_dirtyFlags |= DirtyStateFlags::ScissorRect;
		_ctx->m_state.m_scissorRect = _rect;
	}
}

void SetViewport(Context* _ctx, gpu::Rect const& _rect, float _minDepth, float _maxDepth)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	if (_ctx->m_state.m_viewport.m_depthMax != _maxDepth
		|| _ctx->m_state.m_viewport.m_depthMin != _minDepth
		|| _ctx->m_state.m_viewport.m_rect.m_bottomRight != _rect.m_bottomRight
		|| _ctx->m_state.m_viewport.m_rect.m_topLeft != _rect.m_topLeft)
	{
		_ctx->m_dirtyFlags |= DirtyStateFlags::ScissorRect;
		_ctx->m_state.m_viewport.m_rect = _rect;
		_ctx->m_state.m_viewport.m_depthMin = _minDepth;
		_ctx->m_state.m_viewport.m_depthMax = _maxDepth;
	}
}

}
}