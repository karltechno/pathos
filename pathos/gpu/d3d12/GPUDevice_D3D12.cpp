#include "GPUDevice_D3D12.h"
#include "D3D12_Utils.h"

#include <kt/Macros.h>
#include <kt/Logging.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

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
	KT_UNUSED(_nativeWindowHandle);

	if (_useDebugLayer)
	{
		HRESULT hr;
		ID3D12Debug* d3dDebug = nullptr;
		hr = ::D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&d3dDebug);
		KT_ASSERT(d3dDebug);
		d3dDebug->EnableDebugLayer();
	}

	IDXGIFactory4* dxgiFactory = nullptr;
	UINT createFlags = 0;

	if (_useDebugLayer)
	{
		createFlags |= DXGI_CREATE_FACTORY_DEBUG;

	}

	D3D_CHECK(CreateDXGIFactory2(createFlags, __uuidof(IDXGIFactory4), (void**)&dxgiFactory));
	KT_SCOPE_EXIT(dxgiFactory->Release());

	IDXGIAdapter4* bestAdaptor = GetBestAdaptor(dxgiFactory);
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

	HRESULT hr = ::D3D12CreateDevice(bestAdaptor, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
	if (!SUCCEEDED(hr))
	{
		KT_LOG_ERROR("D3D12CreateDevice failed! (HRESULT: %#x)", hr);
		return false;
	}

	SafeReleaseDX(bestAdaptor);

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
		D3D_CHECK(CreateDXGIFactory2(createFlags, __uuidof(IDXGIFactory4), (void**)&dxgiFactory));

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

		dxgiFactory->CreateSwapChainForHwnd(m_device, HWND(_nativeWindowHandle), &swapChainDesc, nullptr, nullptr, &m_swapChain);
		ID3D12GraphicsCommandList *l;
	}

	return true;
}

void Device_D3D12::Shutdown()
{
	SafeReleaseDX(m_device);
	SafeReleaseDX(m_swapChain);
	
	m_commandQueueManager.Shutdown();
}

}