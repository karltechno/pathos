#pragma once
#include <stdint.h>

#include <kt/Array.h>
#include <kt/Strings.h>
#include <kt/Handles.h>
#include <kt/HashMap.h>

#include "CommandQueue_D3D12.h"
#include "DescriptorHeap_D3D12.h"
#include "Types_D3D12.h"
#include "HandleRef.h"

struct ID3D12Device2;
struct IDXGISwapChain4;
struct IDXGISwapChain1;

namespace gpu
{

struct Device_D3D12;

struct AllocatedObjectBase_D3D12
{
	void Init(char const* _name = nullptr)
	{
#if KT_DEBUG
		m_debugName = _name ? _name : "";
#else
		KT_UNUSED(_name);
#endif
		m_refs = 1;
	}

	void AddRef()
	{
		KT_ASSERT(m_refs);
		++m_refs;
	}

	uint32_t ReleaseRef()
	{
		KT_ASSERT(m_refs);
		return --m_refs;
	}

#if KT_DEBUG
	kt::String64 m_debugName;
#endif
	uint32_t m_refs = 0;
};

struct AllocatedBuffer_D3D12 : AllocatedObjectBase_D3D12
{
	bool Init(BufferDesc const& _desc, void const* _initialData, char const* _debugName = nullptr);
	void Destroy();

	void UpdateViews();

	gpu::BufferDesc m_desc;

	ID3D12Resource* m_res = nullptr;

	// Offset inside of resource.
	uint64_t m_offset = 0;

	// GPU Address (with offset).
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress = 0;

	// Descriptors
	D3D12_CPU_DESCRIPTOR_HANDLE m_srv = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_uav = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_cbv = {};

	// Current state.
	D3D12_RESOURCE_STATES m_state = D3D12_RESOURCE_STATE_COMMON;

	// CPU pointer (if host visible)
	void* m_mappedCpuData = nullptr;

	uint32_t m_lastFrameTouched = 0xFFFFFFFF;

	bool m_ownsResource = false;
};

struct AllocatedTexture_D3D12 : AllocatedObjectBase_D3D12
{
	bool Init(TextureDesc const& _desc, void const* _initialData, char const* _debugName = nullptr);
	void InitFromBackbuffer(ID3D12Resource* _res, gpu::Format _format, uint32_t _height, uint32_t _width);
	void Destroy();

	gpu::TextureDesc m_desc;
	ID3D12Resource* m_res = nullptr;

	D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE m_srv = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtv = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsv = {};

	D3D12_RESOURCE_STATES m_state = D3D12_RESOURCE_STATE_COMMON;

	bool m_ownsResource = false;
};

struct AllocatedShader_D3D12;


struct AllocatedGraphicsPSO_D3D12 : AllocatedObjectBase_D3D12
{
	void Init(ID3D12Device* _device, gpu::GraphicsPSODesc const& _desc, gpu::ShaderBytecode const& _vs, gpu::ShaderBytecode const& _ps);
	void Destroy();

	gpu::GraphicsPSODesc m_psoDesc;

	ID3D12PipelineState* m_pso = nullptr;
};

struct FrameUploadPage_D3D12
{
	uint32_t SizeLeft() const { KT_ASSERT(m_size >= m_curOffset); return m_size - m_curOffset; }

	void* m_mappedPtr = nullptr;
	ID3D12Resource* m_res = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_base = { 0 };
	uint32_t m_curOffset = 0;
	uint32_t m_size = 0;
};

struct FrameUploadPagePool_D3D12
{
	static uint32_t constexpr c_defaultPageSize = 16 * 1024 * 1024;

	void Init(ID3D12Device* _device);
	void Shutdown();

	FrameUploadPage_D3D12 AllocPage(uint32_t _minSize);
	void ReleasePage(FrameUploadPage_D3D12 const& _page);

private:
	ID3D12Device* m_device = nullptr;
	kt::Array<FrameUploadPage_D3D12> m_freePages;
};

struct ScratchAlloc_D3D12
{
	ID3D12Resource* m_res;
	D3D12_GPU_VIRTUAL_ADDRESS m_addr;
	uint64_t m_offset;
	void* m_cpuData;
};

struct FrameUploadAllocator_D3D12
{
	void Init(FrameUploadPagePool_D3D12* _pagePool);
	void Shutdown();
	
	void ClearOnBeginFrame();

	void Alloc(AllocatedBuffer_D3D12& o_res);
	ScratchAlloc_D3D12 Alloc(uint32_t _size, uint32_t _align);

private:
	kt::Array<FrameUploadPage_D3D12> m_pages;
	uint32_t m_numFullPages = 0;
	FrameUploadPagePool_D3D12* m_pagePool;
};


extern Device_D3D12* g_device;

struct Device_D3D12
{
	KT_NO_COPY(Device_D3D12);

	Device_D3D12();
	~Device_D3D12();

	void Init(void* _nativeWindowHandle, bool _useDebugLayer);

	void TestOneFrame();

	void BeginFrame();
	void EndFrame();

	struct FrameResources
	{
		void ClearOnBeginFrame();

		FrameUploadAllocator_D3D12 m_uploadAllocator;
		kt::Array<ID3D12DeviceChild*> m_deferredDeletions;
	};

	using PSOCache = kt::HashMap<uint64_t, GraphicsPSORef, kt::HashMap_KeyOps_IdentityInt<uint64_t>>;

	void DebugCreateGraphicsPSO();

	FrameResources* GetFrameResources()
	{
		return &m_framesResources[m_cpuFrameIdx];
	}

	kt::String256 m_deviceName;

	ID3D12PipelineState* m_debugPsoTest = nullptr;
	ID3D12RootSignature* m_debugRootSig = nullptr;

	ID3D12Device2* m_d3dDev = nullptr;
	IDXGISwapChain1* m_swapChain = nullptr;
	
	gpu::TextureRef m_backBuffers[c_maxBufferedFrames];

	gpu::TextureRef m_backbufferDepth;

	CommandQueueManager_D3D12 m_commandQueueManager;

	FreeListDescriptorHeap_D3D12 m_rtvHeap;
	FreeListDescriptorHeap_D3D12 m_dsvHeap;
	FreeListDescriptorHeap_D3D12 m_stagingHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE m_nullCbv;

	FrameUploadPagePool_D3D12 m_uploadPagePool;

	FrameResources m_framesResources[c_maxBufferedFrames];

	DescriptorHeap_D3D12 m_cbvsrvuavHeap;
	RingBufferDescriptorHeap_D3D12 m_descriptorcbvsrvuavRingBuffer;

	kt::VersionedHandlePool<AllocatedBuffer_D3D12>		m_bufferHandles;
	kt::VersionedHandlePool<AllocatedTexture_D3D12>		m_textureHandles;
	kt::VersionedHandlePool<AllocatedShader_D3D12>		m_shaderHandles;
	kt::VersionedHandlePool<AllocatedGraphicsPSO_D3D12>	m_psoHandles;

	PSOCache m_psoCache;

	uint64_t m_frameFences[c_maxBufferedFrames] = {};

	uint32_t m_cpuFrameIdx = 0;

	uint32_t m_swapChainWidth = 0;
	uint32_t m_swapChainHeight = 0;

	uint32_t m_frameCounter = 0;

	bool m_withDebugLayer = false;
};

}