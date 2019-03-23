#include "CommandQueue.h"
#include "D3D12_Utils.h"

#include <kt/Strings.h>

namespace gpu
{

static uint64_t ShiftFenceMask(D3D12_COMMAND_LIST_TYPE _ty)
{
	return uint64_t(_ty) << 56ull;
}

static D3D12_COMMAND_LIST_TYPE FenceToQueueType(uint64_t _val)
{
	return D3D12_COMMAND_LIST_TYPE(_val >> 56ull);
}

void CommandQueue_D3D12::Init(ID3D12Device* _dev, D3D12_COMMAND_LIST_TYPE _ty)
{
	m_queueType = _ty;
	m_nextFenceVal = ShiftFenceMask(_ty) + 1;
	m_lastCompletedFenceVal = ShiftFenceMask(_ty);

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = _ty;
	D3D_CHECK(_dev->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)));
	
	switch (_ty)
	{
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: D3D_SET_DEBUG_NAME(m_commandQueue, "Command Queue: Compute"); break;
		case D3D12_COMMAND_LIST_TYPE_COPY: D3D_SET_DEBUG_NAME(m_commandQueue, "Command Queue: Copy"); break;
		case D3D12_COMMAND_LIST_TYPE_DIRECT: D3D_SET_DEBUG_NAME(m_commandQueue, "Command Queue: Direct"); break;
		default: D3D_SET_DEBUG_NAME(m_commandQueue, "Command Queue"); break;
	}

	D3D_CHECK(_dev->CreateFence(m_lastCompletedFenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	
	m_waitEvent = ::CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	KT_ASSERT(m_waitEvent != INVALID_HANDLE_VALUE);
}

void CommandQueue_D3D12::Shutdown()
{
	SafeReleaseDX(m_commandQueue);
	SafeReleaseDX(m_fence);

	if (m_waitEvent) { ::CloseHandle(m_waitEvent); m_waitEvent = INVALID_HANDLE_VALUE; }
}

void CommandQueue_D3D12::InsertWaitForFence(uint64_t _fenceVal)
{
	D3D_CHECK(m_commandQueue->Wait(m_fence, _fenceVal));
}

void CommandQueue_D3D12::InsertWaitForOtherQueueFence(CommandQueue_D3D12& _other, uint64_t _fenceVal)
{
	D3D_CHECK(m_commandQueue->Wait(_other.D3DFence(), _fenceVal));
}

void CommandQueue_D3D12::InsertWaitForOtherQueue(CommandQueue_D3D12& _other)
{
	D3D_CHECK(m_commandQueue->Wait(_other.D3DFence(), _other.NextFenceValue()))
}

uint64_t CommandQueue_D3D12::ExecuteCommandLists(ID3D12CommandList** _lists, uint32_t _numLists)
{
	for (uint32_t i = 0; i < _numLists; ++i)
	{
		ID3D12GraphicsCommandList* list = (ID3D12GraphicsCommandList*)_lists[i];
		D3D_CHECK(list->Close());
	}
	m_commandQueue->ExecuteCommandLists(_numLists, _lists);
	m_commandQueue->Signal(m_fence, m_nextFenceVal);
	++m_nextFenceVal;
}

bool CommandQueue_D3D12::HasFenceCompleted(uint64_t _fenceVal)
{
	if (_fenceVal > m_lastCompletedFenceVal)
	{
		// Lazily update current fence value.
		CurrentFenceValue();
	}

	return _fenceVal <= m_lastCompletedFenceVal;
}

uint64_t CommandQueue_D3D12::CurrentFenceValue()
{
	m_lastCompletedFenceVal = kt::Max<uint64_t>(m_lastCompletedFenceVal, m_fence->GetCompletedValue());
	return m_lastCompletedFenceVal;
}

uint64_t CommandQueue_D3D12::NextFenceValue()
{
	return m_nextFenceVal;
}

void CommandQueue_D3D12::WaitForFenceBlockingCPU(uint64_t _fenceVal)
{
	if (HasFenceCompleted(_fenceVal))
	{
		return;
	}

	{
		// Todo: threading.
		m_fence->SetEventOnCompletion(_fenceVal, m_waitEvent);
		::WaitForSingleObjectEx(m_waitEvent, INFINITE, false);
		m_lastCompletedFenceVal = _fenceVal;
	}
}

ID3D12Fence* CommandQueue_D3D12::D3DFence()
{
	return m_fence;
}

void CommandQueueManager_D3D12::Init(ID3D12Device* _dev)
{
	m_computeQueue.Init(_dev, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	m_graphicsQueue.Init(_dev, D3D12_COMMAND_LIST_TYPE_DIRECT);
	m_copyQueue.Init(_dev, D3D12_COMMAND_LIST_TYPE_COPY);
}

void CommandQueueManager_D3D12::Shutdown()
{
	m_computeQueue.Shutdown();
	m_graphicsQueue.Shutdown();
	m_copyQueue.Shutdown();
}

CommandQueue_D3D12& CommandQueueManager_D3D12::QueueByType(D3D12_COMMAND_LIST_TYPE _ty)
{
	switch (_ty)
	{
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: return m_computeQueue;
		case D3D12_COMMAND_LIST_TYPE_COPY: return m_copyQueue;
		case D3D12_COMMAND_LIST_TYPE_DIRECT: return m_graphicsQueue;
	
		default: KT_ASSERT(!"Bad D3D12_COMMAND_LIST_TYPE.");
	}

	KT_UNREACHABLE;
}

bool CommandQueueManager_D3D12::HasFenceCompleted(uint64_t _fence)
{
	return QueueByType(FenceToQueueType(_fence)).HasFenceCompleted(_fence);
}

void CommandQueueManager_D3D12::FlushAllBlockingCPU()
{
	m_copyQueue.FlushBlockingCPU();
	m_computeQueue.FlushBlockingCPU();
	m_graphicsQueue.FlushBlockingCPU();
}

}