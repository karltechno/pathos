#pragma once
#include <d3d12.h>
#include <stdint.h>

namespace gpu
{

class CommandQueue_D3D12
{
public:
	void Init(ID3D12Device* _dev, CommandQueueManager_D3D12* _manager, D3D12_COMMAND_LIST_TYPE _ty);
	void Shutdown();

	uint64_t InsertAndIncrementFence();

	void WaitForFenceGPU(uint64_t _fenceVal);
	void WaitForQueueGPU(CommandQueue_D3D12& _other);

	uint64_t ExecuteCommandLists(ID3D12CommandList** _lists, uint32_t _numLists);

	bool HasFenceCompleted(uint64_t _fenceVal);

	uint64_t CurrentFenceValue();
	uint64_t NextFenceValue();

	void WaitForFenceBlockingCPU(uint64_t _fenceVal);
	void FlushBlockingCPU();

	ID3D12Fence* D3DFence();
	ID3D12CommandQueue* D3DCommandQueue();

private:
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
public:
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