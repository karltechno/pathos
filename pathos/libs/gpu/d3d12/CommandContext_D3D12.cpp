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
		KT_ASSERT(!(_allocatedBuffer->m_bufferDesc.m_flags & BufferFlags::Transient) || _allocatedBuffer->m_lastFrameTouched == _ctx->m_device->m_frameCounter); \
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

	if (!!(m_cmdListFlags & CommandListFlags_D3D12::Compute))
	{
		m_cmdList->SetComputeRootSignature(m_device->m_computeRootSig);
	}

	if (!!(m_cmdListFlags & CommandListFlags_D3D12::Graphics))
	{
		m_cmdList->SetGraphicsRootSignature(m_device->m_graphicsRootSig);
	}
}

CommandContext_D3D12::~CommandContext_D3D12()
{
	// Todo: reclaim?
	KT_ASSERT(m_state.m_pendingUploads.Size() == 0);
	SafeReleaseDX(m_cmdList);
}

void End(Context* _ctx)
{
	FlushBarriers(_ctx);
	ID3D12CommandList* list = _ctx->m_cmdList;

	uint64_t const fence = _ctx->m_device->m_commandQueueManager.QueueByType(_ctx->m_d3dType).ExecuteCommandLists(kt::MakeSlice(list));
	_ctx->m_device->m_commandQueueManager.QueueByType(_ctx->m_d3dType).ReleaseAllocator(_ctx->m_cmdAllocator, fence);
	_ctx->m_cmdAllocator = nullptr;

	delete _ctx;
}

static void MarkDirtyIfBound(Context* _ctx, gpu::ResourceHandle _handle, AllocatedResource_D3D12* _res)
{
	// TODO: This is completely broken with descriptor tables currently since they aren't tracked.
	// That code is temp and need to be-reworked anyway.
	KT_ASSERT(!_res->IsTexture());
	if (!!(_res->m_bufferDesc.m_flags & BufferFlags::Vertex))
	{
		for (gpu::BufferRef const hndl : _ctx->m_state.m_vertexStreams)
		{
			if (hndl.Handle() == _handle)
			{
				_ctx->m_dirtyFlags |= DirtyStateFlags::VertexBuffer;
				break;
			}
		}
	}

	if (!!(_res->m_bufferDesc.m_flags & BufferFlags::Index))
	{
		if (_ctx->m_state.m_indexBuffer.Handle() == _handle)
		{
			_ctx->m_dirtyFlags |= DirtyStateFlags::IndexBuffer;
		}
	}
}

void SetVertexBuffer(Context* _ctx, uint32_t _streamIdx, gpu::BufferHandle _handle)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	KT_ASSERT(_streamIdx < gpu::c_maxVertexStreams);
	if (_ctx->m_state.m_vertexStreams[_streamIdx].Handle() != _handle)
	{
#if KT_DEBUG
		if (_handle.IsValid())
		{
			AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
			KT_ASSERT(res);
			KT_ASSERT(!res->IsTexture());
			KT_ASSERT(!!(res->m_bufferDesc.m_flags & gpu::BufferFlags::Vertex));
		}
#endif

		_ctx->m_state.m_vertexStreams[_streamIdx].Acquire(_handle);
		_ctx->m_dirtyFlags |= DirtyStateFlags::VertexBuffer;
	}
}

void SetIndexBuffer(Context* _ctx, gpu::BufferHandle _handle)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	if (_ctx->m_state.m_indexBuffer.Handle() != _handle)
	{
#if KT_DEBUG
		if (_handle.IsValid())
		{
			AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
			KT_ASSERT(res);
			KT_ASSERT(!res->IsTexture());
			KT_ASSERT(!!(res->m_bufferDesc.m_flags & gpu::BufferFlags::Index));
		}
#endif

		_ctx->m_state.m_indexBuffer.Acquire(_handle);
		_ctx->m_dirtyFlags |= DirtyStateFlags::IndexBuffer;
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE MakeCBVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors)
{
	D3D12_GPU_DESCRIPTOR_HANDLE tableDestGpu;
	D3D12_CPU_DESCRIPTOR_HANDLE tableDestCpu;
	uint32_t const tableSize = _descriptors.Size();
	_ctx->m_device->m_descriptorcbvsrvuavRingBuffer.Alloc(tableSize, tableDestCpu, tableDestGpu);

	uint32_t const descriptorIncrement = _ctx->m_device->m_cbvsrvuavHeap.m_descriptorIncrementSize;

	for (DescriptorData const& descriptor : _descriptors)
	{
		if (descriptor.m_type == DescriptorData::Type::View)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
			if (AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(descriptor.res.m_handle))
			{
				KT_ASSERT(res->m_cbv.ptr);
				cpuHandle = res->m_cbv;
			}
			else
			{
				cpuHandle = _ctx->m_device->m_nullCbv;
			}
			// TODO: Could use the non simple version with one call?
			_ctx->m_device->m_d3dDev->CopyDescriptorsSimple(1, tableDestCpu, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		else
		{
			KT_ASSERT(descriptor.m_type == DescriptorData::Type::ScratchConstant);

			ScratchAlloc_D3D12 constantScratch = _ctx->m_device->GetFrameResources()->m_uploadAllocator.Alloc(descriptor.constants.m_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			memcpy(constantScratch.m_cpuData, descriptor.constants.m_ptr, descriptor.constants.m_size);

			// Write directly into the descriptor ring allocation
			// TODO: Should either change the global root sig so we have a couple gpu virtual addresses for hot constant buffers or allow customisable root sigs.
			// According to nvidia docs this can have some good perf advantages.
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
			cbvDesc.BufferLocation = constantScratch.m_addr;
			cbvDesc.SizeInBytes = UINT(kt::AlignUp(descriptor.constants.m_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
			_ctx->m_device->m_d3dDev->CreateConstantBufferView(&cbvDesc, tableDestCpu);
		}

		tableDestCpu.ptr += descriptorIncrement;
	}

	return tableDestGpu;
}

static D3D12_GPU_DESCRIPTOR_HANDLE MakeUAVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors)
{
	D3D12_GPU_DESCRIPTOR_HANDLE tableDestGpu;
	D3D12_CPU_DESCRIPTOR_HANDLE tableDestCpu;
	uint32_t const tableSize = _descriptors.Size();
	_ctx->m_device->m_descriptorcbvsrvuavRingBuffer.Alloc(tableSize, tableDestCpu, tableDestGpu);

	uint32_t const descriptorIncrement = _ctx->m_device->m_cbvsrvuavHeap.m_descriptorIncrementSize;

	for (DescriptorData const& descriptor : _descriptors)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		KT_ASSERT(descriptor.m_type == DescriptorData::Type::View);
		if (AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(descriptor.res.m_handle))
		{
			KT_ASSERT(descriptor.res.m_uavMipIdx < res->m_uavs.Size());
			KT_ASSERT(res->m_srv.ptr);
			cpuHandle = res->m_uavs[descriptor.res.m_uavMipIdx];
		}
		else
		{
			cpuHandle = _ctx->m_device->m_nullUav;
		}
		// TODO: Could use the non simple version with one call?
		_ctx->m_device->m_d3dDev->CopyDescriptorsSimple(1, tableDestCpu, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		tableDestCpu.ptr += descriptorIncrement;
	}

	return tableDestGpu;
}


static D3D12_GPU_DESCRIPTOR_HANDLE MakeSRVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors)
{
	D3D12_GPU_DESCRIPTOR_HANDLE tableDestGpu;
	D3D12_CPU_DESCRIPTOR_HANDLE tableDestCpu;
	uint32_t const tableSize = _descriptors.Size();
	_ctx->m_device->m_descriptorcbvsrvuavRingBuffer.Alloc(tableSize, tableDestCpu, tableDestGpu);

	uint32_t const descriptorIncrement = _ctx->m_device->m_cbvsrvuavHeap.m_descriptorIncrementSize;

	for (DescriptorData const& descriptor : _descriptors)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		KT_ASSERT(descriptor.m_type == DescriptorData::Type::View);
		if (AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(descriptor.res.m_handle))
		{
			if (res->m_type == ResourceType::TextureCube && descriptor.res.m_srvCubeAsArray)
			{
				KT_ASSERT(res->m_srv.ptr);
				cpuHandle = res->m_srvCubeAsArray;
			}
			else
			{
				KT_ASSERT(res->m_srv.ptr);
				cpuHandle = res->m_srv;
			}
		}
		else
		{
			cpuHandle = _ctx->m_device->m_nullSrv;
		}
		// TODO: Could use the non simple version with one call?
		_ctx->m_device->m_d3dDev->CopyDescriptorsSimple(1, tableDestCpu, cpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		tableDestCpu.ptr += descriptorIncrement;
	}
	
	return tableDestGpu;
}


void SetComputeCBVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space)
{
	// TODO: Magic numbers
	_ctx->m_cmdList->SetComputeRootDescriptorTable(3 * _space, MakeCBVTable(_ctx, _descriptors));
}

void SetComputeSRVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space)
{
	// TODO: Magic numbers
	_ctx->m_cmdList->SetComputeRootDescriptorTable(1 + 3 * _space, MakeSRVTable(_ctx, _descriptors));
}

void SetComputeUAVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space)
{
	// TODO: Magic numbers
	_ctx->m_cmdList->SetComputeRootDescriptorTable(2 + 3 * _space, MakeUAVTable(_ctx, _descriptors));
}


void SetGraphicsCBVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space)
{
	// TODO: Magic numbers
	_ctx->m_cmdList->SetGraphicsRootDescriptorTable(3 * _space, MakeCBVTable(_ctx, _descriptors));
}

void SetGraphicsUAVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space)
{
	// TODO: Magic numbers
	_ctx->m_cmdList->SetGraphicsRootDescriptorTable(2 + 3 * _space, MakeUAVTable(_ctx, _descriptors));
}

void SetGraphicsSRVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space)
{
	// TODO: Magic numbers
	_ctx->m_cmdList->SetGraphicsRootDescriptorTable(1 + 3 * _space, MakeSRVTable(_ctx, _descriptors));
}

void DrawIndexedInstanced(Context* _ctx, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startIndex, uint32_t _baseVertex, uint32_t _startInstance)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	FlushBarriers(_ctx);
	_ctx->ApplyStateChanges(CommandListFlags_D3D12::Graphics);
	_ctx->m_cmdList->DrawIndexedInstanced(_indexCount, _instanceCount, _startIndex, _baseVertex, _startInstance);
}

void DrawInstanced(Context* _ctx, uint32_t _vertexCount, uint32_t _instanceCount, uint32_t _startVertex, uint32_t _startInstance)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	FlushBarriers(_ctx);
	_ctx->ApplyStateChanges(CommandListFlags_D3D12::Graphics);
	_ctx->m_cmdList->DrawInstanced(_vertexCount, _instanceCount, _startVertex, _startInstance);
}

void Dispatch(Context* _ctx, uint32_t _x, uint32_t _y, uint32_t _z)
{
	FlushBarriers(_ctx);
	_ctx->ApplyStateChanges(CommandListFlags_D3D12::Compute);
	_ctx->m_cmdList->Dispatch(_x, _y, _z);
}

void UpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, void const* _mem, uint32_t _size)
{
	kt::Slice<uint8_t> slice = BeginUpdateTransientBuffer(_ctx, _handle, _size);
	memcpy(slice.Data(), _mem, _size);
	EndUpdateTransientBuffer(_ctx, _handle);
}

void UpdateDynamicBuffer(Context* _ctx, gpu::BufferHandle _handle, void const* _mem, uint32_t _size, uint32_t _destOffset)
{
	// Should this be done on the copy queue and synchronized?
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Copy);

	KT_ASSERT(_ctx->m_device->m_resourceHandles.IsValid(_handle));
	AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
	KT_ASSERT(!!(res->m_bufferDesc.m_flags & BufferFlags::Dynamic));
	ScratchAlloc_D3D12 scratch = _ctx->m_device->GetFrameResources()->m_uploadAllocator.Alloc(_size, 16);
	memcpy(scratch.m_cpuData, _mem, _size);
	_ctx->m_cmdList->CopyBufferRegion(res->m_res, _destOffset, scratch.m_res, scratch.m_offset, _size);
}

kt::Slice<uint8_t> BeginUpdateDynamicBuffer(Context* _ctx, gpu::BufferHandle _handle, uint32_t _size, uint32_t _offset)
{
	// Should this be done on the copy queue and synchronized?
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Copy);
	KT_ASSERT(_ctx->m_device->m_resourceHandles.IsValid(_handle));
	AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
	KT_ASSERT(!!(res->m_bufferDesc.m_flags & BufferFlags::Dynamic));

#if KT_DEBUG
	for (CommandContext_D3D12::PendingDynamicUpload const& oldUpdate : _ctx->m_state.m_pendingUploads)
	{
		KT_ASSERT(oldUpdate.m_resource.Handle() != _handle && !"Already updating this buffer!");
	}
#endif

	ScratchAlloc_D3D12 scratch = _ctx->m_device->GetFrameResources()->m_uploadAllocator.Alloc(_size, 16);

	CommandContext_D3D12::PendingDynamicUpload& pendingUpload = _ctx->m_state.m_pendingUploads.PushBack();
	pendingUpload.m_scratch = scratch;
	pendingUpload.m_resource = _handle;
	pendingUpload.m_destOffset = _offset;
	pendingUpload.m_copySize = _size;

	return kt::MakeSlice((uint8_t*)scratch.m_cpuData, _size);
}

void EndUpdateDynamicBuffer(Context* _ctx, gpu::BufferHandle _handle)
{
	KT_ASSERT(_ctx->m_device->m_resourceHandles.IsValid(_handle));

	for (CommandContext_D3D12::PendingDynamicUpload* it = _ctx->m_state.m_pendingUploads.Begin();
		 it != _ctx->m_state.m_pendingUploads.End();
		 ++it)
	{
		if (it->m_resource.Handle() == _handle)
		{
			AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
			_ctx->m_cmdList->CopyBufferRegion(res->m_res, it->m_destOffset, it->m_scratch.m_res, it->m_scratch.m_offset, it->m_copySize);
			_ctx->m_state.m_pendingUploads.EraseSwap(it);
			return;
		}
	}

	KT_ASSERT(!"BeginUpdateDynamicBuffer was not called with this resource.")
}

kt::Slice<uint8_t> BeginUpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, uint32_t _size)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Copy);

	KT_ASSERT(_ctx->m_device->m_resourceHandles.IsValid(_handle));
	AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
	KT_ASSERT(!!(res->m_bufferDesc.m_flags & BufferFlags::Transient));

	res->UpdateTransientSize(_size);

	_ctx->m_device->GetFrameResources()->m_uploadAllocator.Alloc(*res);

	KT_ASSERT(res->m_mappedCpuData);
	res->m_lastFrameTouched = _ctx->m_device->m_frameCounter;
	res->UpdateViews();

	return kt::MakeSlice((uint8_t*)res->m_mappedCpuData, res->m_bufferDesc.m_sizeInBytes);
}

void EndUpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle)
{
	AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
	KT_ASSERT(res);
	MarkDirtyIfBound(_ctx, _handle, res);
}

void SetPSO(Context* _ctx, gpu::PSOHandle _pso)
{
	CHECK_QUEUE_FLAGS(_ctx, CommandListFlags_D3D12::Graphics);

	if (_ctx->m_state.m_pso.Handle() != _pso)
	{
		_ctx->m_state.m_pso.Acquire(_pso);
		_ctx->m_dirtyFlags |= DirtyStateFlags::PipelineState;
	}
}

void ClearRenderTarget(Context* _ctx, gpu::TextureHandle _handle, float const _color[4])
{
	AllocatedResource_D3D12* tex = _ctx->m_device->m_resourceHandles.Lookup(_handle);
	// TODO: Flush barriers for render target
	KT_ASSERT(tex);
	KT_ASSERT(tex->IsTexture());
	KT_ASSERT(tex->m_rtv.ptr);
	_ctx->m_cmdList->ClearRenderTargetView(tex->m_rtv, _color, 0, nullptr);
}

void SetRenderTarget(Context* _ctx, uint32_t _idx, gpu::TextureHandle _handle)
{
	if (_ctx->m_state.m_renderTargets[_idx].Handle() != _handle)
	{
#if KT_DEBUG
		if (_handle.IsValid())
		{
			AllocatedResource_D3D12* tex = _ctx->m_device->m_resourceHandles.Lookup(_handle);
			KT_ASSERT(tex);
			KT_ASSERT(tex->IsTexture());
			KT_ASSERT(tex->m_rtv.ptr);
		}
#endif

		_ctx->m_state.m_renderTargets[_idx].Acquire(_handle);
		_ctx->m_dirtyFlags |= DirtyStateFlags::RenderTarget;
	}
}

void SetDepthBuffer(Context* _ctx, gpu::TextureHandle _handle, uint32_t _arrayIdx, uint32_t _mipIdx)
{
	if (!_ctx->m_state.m_depthBuffer.Equals(_handle, _arrayIdx, _mipIdx))
	{
#if KT_DEBUG
		if (_handle.IsValid())
		{
			AllocatedResource_D3D12* tex = _ctx->m_device->m_resourceHandles.Lookup(_handle);
			KT_ASSERT(tex);
			KT_ASSERT(tex->IsTexture());
		}
#endif

		_ctx->m_state.m_depthBuffer.m_tex = _handle;
		_ctx->m_state.m_depthBuffer.m_arrayIdx = _arrayIdx;
		_ctx->m_state.m_depthBuffer.m_mipIdx = _mipIdx;
		_ctx->m_dirtyFlags |= DirtyStateFlags::DepthBuffer;
	}
}

void ClearDepth(Context* _ctx, gpu::TextureHandle _handle, float _depth, uint32_t _arrayIdx, uint32_t _mipIdx)
{
	AllocatedResource_D3D12* tex = _ctx->m_device->m_resourceHandles.Lookup(_handle);
	// TODO: Flush barriers for depth target?
	KT_ASSERT(tex);
	KT_ASSERT(tex->IsTexture());
	KT_ASSERT(!!(tex->m_textureDesc.m_usageFlags & TextureUsageFlags::DepthStencil));

	uint32_t const descriptorIdx = gpu::D3DSubresourceIndex(_mipIdx, _arrayIdx, tex->m_textureDesc.m_mipLevels);
	KT_ASSERT(descriptorIdx < tex->m_dsvs.Size());
	D3D12_CPU_DESCRIPTOR_HANDLE const descriptor = tex->m_dsvs[descriptorIdx];
	_ctx->m_cmdList->ClearDepthStencilView(descriptor, D3D12_CLEAR_FLAG_DEPTH, _depth, 0, 0, nullptr);
}

void ResourceBarrier(Context* _ctx, gpu::ResourceHandle _handle, gpu::ResourceState _newState)
{
	AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
	KT_ASSERT(res);
	if (res->m_resState == _newState)
	{
		return;
	}

	D3D12_RESOURCE_STATES const newState = gpu::D3DTranslateResourceState(_newState);

	// Check if there are any pending barriers - maybe store in resource?
	for (D3D12_RESOURCE_BARRIER& b : _ctx->m_state.m_batchedBarriers)
	{
		if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION 
			&& b.Transition.pResource == res->m_res)
		{
			// swap the new state if is isn't ours. 
			// TODO: This is pretty blunt, and might break - consider revisiting this.
			b.Transition.StateAfter = newState;
			// force to all subresources
			b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			return;
		}
	}

	D3D12_RESOURCE_BARRIER& d3dBarrier = _ctx->m_state.m_batchedBarriers.PushBack();
	d3dBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE; // TODO: Split barriers (probably easiest with rendergraph-esque abstraction).
	d3dBarrier.Transition.StateBefore = gpu::D3DTranslateResourceState(res->m_resState);
	d3dBarrier.Transition.StateAfter = newState;
	d3dBarrier.Transition.pResource = res->m_res;
	d3dBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// TODO: doesn't play nice with threading, or redundant barriers
	res->m_resState = _newState;
}

void UAVBarrier(Context* _ctx, gpu::ResourceHandle _handle)
{
	AllocatedResource_D3D12* res = _ctx->m_device->m_resourceHandles.Lookup(_handle);
	KT_ASSERT(res);

	D3D12_RESOURCE_BARRIER& barrier = _ctx->m_state.m_batchedBarriers.PushBack();
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = res->m_res;
}

D3D12_GPU_DESCRIPTOR_HANDLE AllocAndCopyFromDescriptorRing(CommandContext_D3D12* _ctx, kt::Slice<D3D12_CPU_DESCRIPTOR_HANDLE> const& _srcHandles)
{
	UINT const tableSize = _srcHandles.Size();
	UINT* sourceSizes = (UINT*)KT_ALLOCA(tableSize * sizeof(UINT));
	for (uint32_t i = 0; i < tableSize; ++i)
	{
		sourceSizes[i] = 1;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE tableDestGpu;
	D3D12_CPU_DESCRIPTOR_HANDLE tableDestCpu;
	UINT const destSize = _srcHandles.Size();
	_ctx->m_device->m_descriptorcbvsrvuavRingBuffer.Alloc(tableSize, tableDestCpu, tableDestGpu);
	_ctx->m_device->m_d3dDev->CopyDescriptors(1, &tableDestCpu, &destSize, tableSize, _srcHandles.Data(), sourceSizes, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return tableDestGpu;
}

void CommandContext_D3D12::ApplyStateChanges(CommandListFlags_D3D12 _dispatchType)
{
	if (!!(m_dirtyFlags & DirtyStateFlags::PipelineState))
	{
		AllocatedPSO_D3D12* pso = m_device->m_psoHandles.Lookup(m_state.m_pso);
		KT_ASSERT(pso);
		KT_ASSERT(pso->IsCompute() == (_dispatchType == CommandListFlags_D3D12::Compute) && "Compute PSO set with draw dispatch!");
		KT_ASSERT(!pso->IsCompute() == (_dispatchType == CommandListFlags_D3D12::Graphics) && "Graphics PSO set with compute dispatch!");
		m_cmdList->SetPipelineState(pso->m_pso);
		m_dirtyFlags &= ~(DirtyStateFlags::PipelineState);
		if (!pso->IsCompute())
		{
			m_state.m_numRenderTargets = pso->m_psoDesc.m_numRenderTargets;
			// Check primitive topology too
			if (pso->m_psoDesc.m_primType != m_state.m_primitive)
			{
				m_state.m_primitive = pso->m_psoDesc.m_primType;
				m_cmdList->IASetPrimitiveTopology(gpu::ToD3DPrimType(m_state.m_primitive));
			}
		}
		else
		{
			m_state.m_numRenderTargets = 0;
		}
	}

	if (!!(_dispatchType & CommandListFlags_D3D12::Graphics))
	{
		ApplyGraphicsStateChanges();
		m_dirtyFlags = DirtyStateFlags::None;
	}
}

void FlushBarriers(Context* _ctx)
{
	if (_ctx->m_state.m_batchedBarriers.Size() == 0)
	{
		return;
	}

	_ctx->m_cmdList->ResourceBarrier(_ctx->m_state.m_batchedBarriers.Size(), _ctx->m_state.m_batchedBarriers.Data());
	_ctx->m_state.m_batchedBarriers.Clear();
}

void CopyResource(Context* _ctx, gpu::ResourceHandle _src, gpu::ResourceHandle _dest)
{
	AllocatedResource_D3D12* resSrc = _ctx->m_device->m_resourceHandles.Lookup(_src);
	AllocatedResource_D3D12* resDst = _ctx->m_device->m_resourceHandles.Lookup(_dest);
	KT_ASSERT(resSrc);
	KT_ASSERT(resDst);
	_ctx->m_cmdList->CopyResource(resDst->m_res, resSrc->m_res);
}

void CommandContext_D3D12::ApplyGraphicsStateChanges()
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
		rect.bottom = LONG(m_state.m_scissorRect.m_bottomRight.y);
		rect.right = LONG(m_state.m_scissorRect.m_bottomRight.x);
		rect.top = LONG(m_state.m_scissorRect.m_topLeft.y);
		rect.left = LONG(m_state.m_scissorRect.m_topLeft.x);
		m_cmdList->RSSetScissorRects(1, &rect);
	}

	if (!!(m_dirtyFlags & DirtyStateFlags::IndexBuffer))
	{
		D3D12_INDEX_BUFFER_VIEW idxView = {};
		if (m_state.m_indexBuffer.IsValid())
		{
			AllocatedResource_D3D12* idxBuff = m_device->m_resourceHandles.Lookup(m_state.m_indexBuffer);
			KT_ASSERT(idxBuff);
			CHECK_TRANSIENT_TOUCHED_THIS_FRAME(this, idxBuff);

			idxView.BufferLocation = idxBuff->m_gpuAddress;
			idxView.Format = ToDXGIFormat(idxBuff->m_bufferDesc.m_format);
			idxView.SizeInBytes = idxBuff->m_bufferDesc.m_sizeInBytes;
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

				AllocatedResource_D3D12* bufferRes = m_device->m_resourceHandles.Lookup(m_state.m_vertexStreams[i]);
				KT_ASSERT(bufferRes);
				KT_ASSERT(!!(bufferRes->m_bufferDesc.m_flags & BufferFlags::Vertex));
				CHECK_TRANSIENT_TOUCHED_THIS_FRAME(this, bufferRes);

				bufferViews[i].BufferLocation = bufferRes->m_gpuAddress;
				bufferViews[i].SizeInBytes = bufferRes->m_bufferDesc.m_sizeInBytes;
				bufferViews[i].StrideInBytes = bufferRes->m_bufferDesc.m_strideInBytes;
			}
		}
		m_cmdList->IASetVertexBuffers(0, maxCount, bufferViews);
	}

	if (!!(m_dirtyFlags & (DirtyStateFlags::DepthBuffer | DirtyStateFlags::RenderTarget)))
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
		D3D12_CPU_DESCRIPTOR_HANDLE rtvs[gpu::c_maxRenderTargets] = {};
		if (m_state.m_depthBuffer.m_tex.IsValid())
		{
			AllocatedResource_D3D12* tex = m_device->m_resourceHandles.Lookup(m_state.m_depthBuffer.m_tex);
			KT_ASSERT(tex);

			uint32_t const descriptorIdx = gpu::D3DSubresourceIndex(m_state.m_depthBuffer.m_mipIdx, m_state.m_depthBuffer.m_arrayIdx, tex->m_textureDesc.m_mipLevels);
			KT_ASSERT(descriptorIdx < tex->m_dsvs.Size());
			dsv = tex->m_dsvs[descriptorIdx];
		}

		uint32_t const numRenderTargets = m_state.m_numRenderTargets;
		for (uint32_t i = 0; i < numRenderTargets; ++i)
		{
			KT_ASSERT(m_state.m_renderTargets[i].IsValid());
			AllocatedResource_D3D12* tex = m_device->m_resourceHandles.Lookup(m_state.m_renderTargets[i]);
			KT_ASSERT(tex);
			KT_ASSERT(tex->m_rtv.ptr);
			rtvs[i] = tex->m_rtv;
		}
		m_cmdList->OMSetRenderTargets(numRenderTargets, rtvs, FALSE, dsv.ptr ? &dsv : nullptr);
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
		_ctx->m_dirtyFlags |= DirtyStateFlags::ViewPort;
		_ctx->m_state.m_viewport.m_rect = _rect;
		_ctx->m_state.m_viewport.m_depthMin = _minDepth;
		_ctx->m_state.m_viewport.m_depthMax = _maxDepth;
	}
}

}
}