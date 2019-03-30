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

	DebugCreateGraphicsPSO();

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

#if 0
	FLOAT rgba[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	gfxList->ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE{ m_backBuffers[m_cpuFrameIdx].m_rtv.ptr }, rgba, 0, nullptr);
#else
	gfxList->SetPipelineState(m_debugPsoTest);
	gfxList->SetGraphicsRootSignature(m_debugRootSig);
	auto rtv = D3D12_CPU_DESCRIPTOR_HANDLE{ m_backBuffers[m_cpuFrameIdx].m_rtv.ptr };

	D3D12_VIEWPORT viewPort;
	viewPort.Width = 1280.0f;
	viewPort.Height = 720.0f;
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;
	viewPort.TopLeftX = 0.0f;
	viewPort.TopLeftY = 0.0f;

	D3D12_RECT rect;
	rect.bottom = 720;
	rect.left = 0;
	rect.top = 0;
	rect.right = 1280;

	gfxList->RSSetScissorRects(1, &rect);

	gfxList->RSSetViewports(1, &viewPort);
	gfxList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	gfxList->IASetIndexBuffer(nullptr);
	gfxList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxList->IASetVertexBuffers(0, 0, nullptr);
	gfxList->DrawInstanced(3, 1, 0, 0);
#endif

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

static void DebugReadEntireFile(FILE* _f, void const*& o_ptr, size_t& o_size)
{
	fseek(_f, 0, SEEK_END);
	size_t len = ftell(_f);
	fseek(_f, 0, SEEK_SET);
	void* ptr = kt::Malloc(len);
	fread(ptr, len, 1, _f);
	o_size = len;
	o_ptr = ptr;
}

void Device_D3D12::DebugCreateGraphicsPSO()
{
	FILE* pshFile = fopen("shaders/test.pixel.cso", "rb");
	FILE* vshFile = fopen("shaders/test.vertex.cso", "rb");

	KT_ASSERT(pshFile);
	KT_ASSERT(vshFile);

	KT_SCOPE_EXIT(fclose(pshFile));
	KT_SCOPE_EXIT(fclose(vshFile));

	D3D12_SHADER_BYTECODE vsh;
	D3D12_SHADER_BYTECODE psh;

	DebugReadEntireFile(vshFile, vsh.pShaderBytecode, vsh.BytecodeLength);
	DebugReadEntireFile(pshFile, psh.pShaderBytecode, psh.BytecodeLength);
	
	{
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
		desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

		desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		
		D3D12_ROOT_PARAMETER1 params[1];
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Constants.RegisterSpace = 0;
		params[0].Constants.ShaderRegister = 0;
		params[0].Constants.Num32BitValues = 0;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	

		desc.Desc_1_1.pParameters = params;
		desc.Desc_1_1.NumParameters = KT_ARRAY_COUNT(params);

		ID3DBlob *rootBlob, *errBlob;

		D3D_CHECK(D3D12SerializeVersionedRootSignature(&desc, &rootBlob, &errBlob));

		if (errBlob)
		{
			KT_LOG_ERROR("Failed to create root signature: %s", (char const*)errBlob->GetBufferPointer());
		}
		else
		{
			D3D_CHECK(m_device->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&m_debugRootSig)));
		}

	}

	{
		static D3D12_DEPTH_STENCILOP_DESC const s_defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.VS = vsh;
		desc.PS = psh;

		static D3D12_RENDER_TARGET_BLEND_DESC const s_defaultRenderTargetBlendDesc =
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};

		desc.BlendState.IndependentBlendEnable = FALSE;
		desc.BlendState.AlphaToCoverageEnable = FALSE;
		desc.BlendState.RenderTarget[0] = s_defaultRenderTargetBlendDesc;

		desc.DepthStencilState.BackFace = s_defaultStencilOp;
		desc.DepthStencilState.FrontFace = s_defaultStencilOp;
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		desc.pRootSignature = m_debugRootSig;

		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.RasterizerState.FrontCounterClockwise = FALSE;
		desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.RasterizerState.DepthClipEnable = TRUE;
		desc.RasterizerState.MultisampleEnable = FALSE;
		desc.RasterizerState.AntialiasedLineEnable = FALSE;
		desc.RasterizerState.ForcedSampleCount = 0;
		desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		desc.InputLayout.NumElements = 0;

		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.SampleMask = 0xFFFFFFFF;

		D3D_CHECK(m_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_debugPsoTest)));
	}
}

}