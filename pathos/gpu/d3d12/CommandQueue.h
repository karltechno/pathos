#pragma once
#include <kt/Array.h>
#include <kt/Macros.h>

#include <d3d12.h>
#include <stdint.h>

namespace gpu
{

class CommandQueueManager_D3D12;

class CommandAllocatorPool_D3D12
{
	KT_NO_COPY(CommandAllocatorPool_D3D12);
public:
	CommandAllocatorPool_D3D12() = default;

	void Init(ID3D12Device* _dev, D3D12_COMMAND_LIST_TYPE _ty);
	void Shutdown();

	ID3D12CommandAllocator* AcquireAllocator(uint64_t _curFenceVal);
	void ReleaseAllocator(ID3D12CommandAllocator* _allocator, uint64_t _fenceVal);

private:
	struct AllocatorAndFence
	{
		ID3D12CommandAllocator* m_allocator;
		uint64_t m_fenceVal;
	};

	ID3D12Device* m_device = nullptr;
	D3D12_COMMAND_LIST_TYPE m_type;

	kt::InplaceArray<AllocatorAndFence, 32> m_pool;
};

class CommandQueue_D3D12
{
	KT_NO_COPY(CommandQueue_D3D12);
public:
	CommandQueue_D3D12() = default;

	void Init(ID3D12Device* _dev, CommandQueueManager_D3D12* _manager, D3D12_COMMAND_LIST_TYPE _ty);
	void Shutdown();

	uint64_t InsertAndIncrementFence();

	void WaitForFenceGPU(uint64_t _fenceVal);
	void WaitForQueueGPU(CommandQueue_D3D12& _other);

	uint64_t ExecuteCommandLists(ID3D12CommandList** _lists, uint32_t _numLists);

	ID3D12CommandAllocator* AcquireAllocator();
	void ReleaseAllocator(ID3D12CommandAllocator* _allocator, uint64_t _fenceVal);

	bool HasFenceCompleted(uint64_t _fenceVal);

	uint64_t CurrentFenceValue();
	uint64_t NextFenceValue();

	void WaitForFenceBlockingCPU(uint64_t _fenceVal);
	void FlushBlockingCPU();

	ID3D12Fence* D3DFence();
	ID3D12CommandQueue* D3DCommandQueue();

private:
	CommandAllocatorPool_D3D12 m_allocatorPool;

	CommandQueueManager_D3D12* m_manager = nullptr;

	ID3D12CommandQueue* m_commandQueue = nullptr;
	D3D12_COMMAND_LIST_TYPE m_queueType;

	ID3D12Fence* m_fence = nullptr;
	uint64_t m_nextFenceVal = 0;
	uint64_t m_lastCompletedFenceVal;

	HANDLE m_waitEvent = INVALID_HANDLE_VALUE;
};

class CommandQueueManager_D3D12
{
	KT_NO_COPY(CommandQueueManager_D3D12);
public:
	CommandQueueManager_D3D12() = default;

	void Init(ID3D12Device* _dev);
	void Shutdown();

	CommandQueue_D3D12& GraphicsQueue() { return m_graphicsQueue; }
	CommandQueue_D3D12& CopyQueue() { return m_copyQueue; }
	CommandQueue_D3D12& ComputeQueue() { return m_computeQueue; }

	CommandQueue_D3D12& QueueByType(D3D12_COMMAND_LIST_TYPE _ty);

	bool HasFenceCompleted(uint64_t _fenceVal);
	void WaitForFenceBlockingCPU(uint64_t _fenceVal);
	void FlushAllBlockingCPU();

private:
	CommandQueue_D3D12 m_graphicsQueue;
	CommandQueue_D3D12 m_copyQueue;
	CommandQueue_D3D12 m_computeQueue;
};

}