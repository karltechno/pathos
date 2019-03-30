#pragma once
#include <stdint.h>

#include <kt/Strings.h>

#include "CommandQueue_D3D12.h"
#include "DescriptorHeap_D3D12.h"
#include "Types_D3D12.h"

struct ID3D12Device2;
struct IDXGISwapChain4;
struct IDXGISwapChain1;

namespace gpu
{

constexpr uint32_t c_d3dBufferedFrames = 3u;

class Device_D3D12
{
public:
	bool Init(void* _nativeWindowHandle, bool _useDebugLayer);
	void Shutdown();

	CommandQueueManager_D3D12& CommandQueueManager() { return m_commandQueueManager; }

	void TestOneFrame();

	void Present();

	void DebugCreateGraphicsPSO();

private:
	kt::String256 m_deviceName;

	ID3D12PipelineState* m_debugPsoTest = nullptr;
	ID3D12RootSignature* m_debugRootSig = nullptr;

	ID3D12Device2* m_device = nullptr;
	IDXGISwapChain1* m_swapChain = nullptr;
	
	CommandQueueManager_D3D12 m_commandQueueManager;

	FreeListDescriptorHeap_D3D12 m_rtvHeap;
	FreeListDescriptorHeap_D3D12 m_dsvHeap;

	FreeListDescriptorHeap_D3D12 m_stagingHeap;
	LinearDescriptorHeap_D3D12 m_frameLinearHeaps[c_d3dBufferedFrames];

	RenderTarget_D3D12 m_backBuffers[c_d3dBufferedFrames];

	uint32_t m_cpuFrameIdx = 0;

	uint32_t m_swapChainWidth = 0;
	uint32_t m_swapChainHeight = 0;

	bool m_withDebugLayer = false;
};

}