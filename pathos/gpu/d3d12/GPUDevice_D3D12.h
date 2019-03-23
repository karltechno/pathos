#pragma once
#include <stdint.h>

#include "CommandQueue.h"

#include <kt/Strings.h>

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

private:
	kt::String256 m_deviceName;

	ID3D12Device2* m_device = nullptr;
	IDXGISwapChain1* m_swapChain = nullptr;
	
	CommandQueueManager_D3D12 m_commandQueueManager;

	uint32_t m_swapChainWidth = 0;
	uint32_t m_swapChainHeight = 0;
};

}