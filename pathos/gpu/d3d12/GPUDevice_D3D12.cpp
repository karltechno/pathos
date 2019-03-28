#include "GPUDevice_D3D12.h"
#include "Utils_D3D12.h"

#include <kt/Macros.h>
#include <kt/Logging.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>


namespace gpu
{

static IDXGIAdapter4* GetBestAdaptor(IDXGIFactory4* _dxgiFactory)
{
	UINT i = 0;
	IDXGIAdapter1* adaptor = nullptr;
	IDXGIAdapter4* adaptor4 = nullptr;
	size_t maxVidMem = 0;

	while (_dxgiFactory->EnumAdapters1(i, &adaptor) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 desc;
		D3D_CHECK(adaptor->GetDesc1(&desc));
		if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			if (SUCCEEDED(D3D12CreateDevice(adaptor, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				if (desc.DedicatedVideoMemory > maxVidMem)
				{
					SafeReleaseDX(adaptor4);
					D3D_CHECK(adaptor->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&adaptor4));
				}
			}
		}

		SafeReleaseDX(adaptor);
		++i;
	}

	return adaptor4;
}



bool Device_D3D12::Init(void* _nativeWindowHandle, bool _useDebugLayer)
{
	m_withDebugLayer = _useDebugLayer;

	if (_useDebugLayer)
	{
		ID3D12Debug* d3dDebug = nullptr;
		D3D_CHECK(::D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&d3dDebug));
		KT_ASSERT(d3dDebug);
		d3dDebug->EnableDebugLayer();
	}


	UINT createFlags = 0;

	if (_useDebugLayer)
	{
		createFlags |= DXGI_CREATE_FACTORY_DEBUG;

	}

	IDXGIFactory4* dxgiFactory = nullptr;
	D3D_CHECK(CreateDXGIFactory2(createFlags, __uuidof(IDXGIFactory4), (void**)&dxgiFactory));
	KT_SCOPE_EXIT(SafeReleaseDX(dxgiFactory));

	IDXGIAdapter4* bestAdaptor = GetBestAdaptor(dxgiFactory);
	KT_SCOPE_EXIT(SafeReleaseDX(bestAdaptor));
	if (!bestAdaptor)
	{
		KT_LOG_ERROR("Failed to find appropriate IDXGIAdapator! Can't init d3d12.");
		return false;
	}


	DXGI_ADAPTER_DESC3 desc;
	bestAdaptor->GetDesc3(&desc);
	desc.DedicatedVideoMemory;

	char nameUtf8[sizeof(desc.Description)];
	::WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameUtf8, sizeof(nameUtf8), nullptr, nullptr);

	m_deviceName = nameUtf8;

	KT_LOG_INFO("Using graphics adaptor: %s, Vendor ID: %u, Device ID: %u", nameUtf8, desc.VendorId, desc.DeviceId);

	HRESULT const hr = ::D3D12CreateDevice(bestAdaptor, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));

	if (!SUCCEEDED(hr))
	{
		KT_LOG_ERROR("D3D12CreateDevice failed! (HRESULT: %#x)", hr);
		return false;
	}

	if (_useDebugLayer)
	{
		ID3D12InfoQueue* infoQueue;
		if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		{
			D3D_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			D3D_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
			D3D_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
			infoQueue->Release();
		}
	}

	m_commandQueueManager.Init(m_device);

	// Todo: capabilities.

	// Swapchain
	{
		RECT r;
		::GetClientRect(HWND(_nativeWindowHandle), &r);

		m_swapChainWidth = r.right - r.left;
		m_swapChainHeight = r.bottom - r.top;

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Height = m_swapChainHeight;
		swapChainDesc.Width = m_swapChainWidth;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO: HDR ?
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = gpu::c_d3dBufferedFrames;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

		D3D_CHECK(dxgiFactory->CreateSwapChainForHwnd(m_commandQueueManager.GraphicsQueue().D3DCommandQueue(), HWND(_nativeWindowHandle), &swapChainDesc, nullptr, nullptr, &m_swapChain));
	}

	// Heaps
	{
		m_rtvHeap.Init(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, false, "Main RTV Heap");
		m_dsvHeap.Init(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, false, "Main DSV Heap");

		m_stagingHeap.Init(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, false, "CBV/SRV/UAV Staging Heap");

		uint32_t heapNum = 1;

		for (LinearDescriptorHeap_D3D12& heap : m_frameLinearHeaps)
		{
			kt::String64 str;
			str.AppendFmt("CBV/SRV/UAV Linear Heap %u", heapNum++);
			heap.Init(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true, str.Data());
		}
	}

	{
		for (uint32_t i = 0; i < c_d3dBufferedFrames; ++i)
		{
			D3D_CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i].m_resource)));
			m_backBuffers[i].m_rtv = m_rtvHeap.AllocOne();
			m_device->CreateRenderTargetView(m_backBuffers[i].m_resource, nullptr, D3D12_CPU_DESCRIPTOR_HANDLE{ m_backBuffers[i].m_rtv.ptr });
		}
	}

	return true;
}

void Device_D3D12::Shutdown()
{
	SafeReleaseDX(m_device);
	SafeReleaseDX(m_swapChain);
	
	m_commandQueueManager.Shutdown();

	m_rtvHeap.Shutdown();
	m_dsvHeap.Shutdown();
	m_stagingHeap.Shutdown();

	for (LinearDescriptorHeap_D3D12& heap : m_frameLinearHeaps)
	{
		heap.Shutdown();
	}

	for (RenderTarget_D3D12& rt : m_backBuffers)
	{
		SafeReleaseDX(rt.m_resource);
		if (rt.m_rtv.ptr)
		{
			m_rtvHeap.Free(rt.m_rtv);
		}
	}
}

void Device_D3D12::TestOneFrame()
{
	ID3D12CommandAllocator* allocator = m_commandQueueManager.GraphicsQueue().AcquireAllocator();
	ID3D12CommandList* list;
	D3D_CHECK(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&list)));
	ID3D12GraphicsCommandList* gfxList = (ID3D12GraphicsCommandList*)list;

	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = m_backBuffers[m_cpuFrameIdx].m_resource;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gfxList->ResourceBarrier(1, &barrier);
	}

	FLOAT rgba[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	gfxList->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE{ m_backBuffers[m_cpuFrameIdx].m_rtv.ptr }, rgba, 0, nullptr);
	
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = m_backBuffers[m_cpuFrameIdx].m_resource;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		gfxList->ResourceBarrier(1, &barrier);
	}

	uint64_t const fence = m_commandQueueManager.GraphicsQueue().ExecuteCommandLists(&list, 1);
	m_commandQueueManager.GraphicsQueue().ReleaseAllocator(allocator, fence);
	gfxList->Release();
	
	Present();
}

void Device_D3D12::Present()
{
	// Todo: vsync
	D3D_CHECK(m_swapChain->Present(0, 0));

	m_cpuFrameIdx = (m_cpuFrameIdx + 1) % gpu::c_d3dBufferedFrames;
}

}