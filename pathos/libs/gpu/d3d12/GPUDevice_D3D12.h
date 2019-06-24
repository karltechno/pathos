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
struct IDXGISwapChain3;
struct IDXGISwapChain1;

#define STORE_GPU_DEBUG_NAME (1) // TODO: Better handling

namespace gpu
{

struct Device_D3D12;

namespace cmd
{
struct CommandContext_D3D12;
}

struct AllocatedObjectBase_D3D12
{
	void Init(char const* _name = nullptr)
	{
#if STORE_GPU_DEBUG_NAME
		m_debugName = (_name && *_name != '\0') ? _name : "Un-named resource";
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

#if STORE_GPU_DEBUG_NAME
	kt::String64 m_debugName;
#endif
	uint32_t m_refs = 0;
};

struct AllocatedResource_D3D12 : AllocatedObjectBase_D3D12
{
	AllocatedResource_D3D12()
		: m_bufferDesc{}
	{}

	bool InitAsBuffer(BufferDesc const& _desc, void const* _initialData, char const* _debugName = nullptr);

	bool InitAsTexture(TextureDesc const& _desc, void const* _initialData, char const* _debugName = nullptr);
	void InitFromBackbuffer(ID3D12Resource* _res, uint32_t _idx, gpu::Format _format, uint32_t _height, uint32_t _width);

	void Destroy();

	void UpdateViews();

	void UpdateTransientSize(uint32_t _size);

	bool IsBuffer() const;
	bool IsTexture() const;

	gpu::ResourceType m_type = gpu::ResourceType::Num_ResourceType;

	union
	{
		gpu::BufferDesc m_bufferDesc;
		gpu::TextureDesc m_textureDesc;
	};

	ID3D12Resource* m_res = nullptr;

	// Offset inside of resource.
	uint64_t m_offset = 0;

	// GPU Address (with offset).
	D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress = 0;

	// Descriptors
	D3D12_CPU_DESCRIPTOR_HANDLE m_srv = {};
	// TODO: Could make view bindings explicit in code.
	D3D12_CPU_DESCRIPTOR_HANDLE m_srvCubeAsArray = {};

	D3D12_CPU_DESCRIPTOR_HANDLE m_cbv = {};
	kt::Array<D3D12_CPU_DESCRIPTOR_HANDLE> m_uavs;

	D3D12_CPU_DESCRIPTOR_HANDLE m_rtv = {};
	D3D12_CPU_DESCRIPTOR_HANDLE m_dsv = {};

	// Current state.
	gpu::ResourceState m_resState = ResourceState::Common;

	// CPU pointer (if host visible)
	void* m_mappedCpuData = nullptr;

	uint32_t m_lastFrameTouched = 0xFFFFFFFF;

	bool m_ownsResource = false;
};

struct AllocatedShader_D3D12;


struct AllocatedPSO_D3D12 : AllocatedObjectBase_D3D12
{
	void InitAsCompute(gpu::ShaderHandle _handle, gpu::ShaderBytecode const _byteCode, char const* _debugName);
	void InitAsGraphics(gpu::GraphicsPSODesc const& _desc, gpu::ShaderBytecode const& _vs, gpu::ShaderBytecode const& _ps, char const* _debugName);
	void Destroy();

	bool IsCompute() const { return m_cs.IsValid(); }

	gpu::GraphicsPSODesc m_psoDesc;
	
	gpu::ShaderHandle m_cs;

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

	void Alloc(AllocatedResource_D3D12& o_res);
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

	void BeginFrame();
	void EndFrame();

	struct FrameResources
	{
		void ClearOnBeginFrame();

		FrameUploadAllocator_D3D12 m_uploadAllocator;
		kt::Array<ID3D12DeviceChild*> m_deferredDeletions;
	};

	using PSOCache = kt::HashMap<uint64_t, PSORef, kt::HashMap_KeyOps_IdentityInt<uint64_t>>;

	FrameResources* GetFrameResources()
	{
		return &m_framesResources[m_cpuFrameIdx];
	}

	kt::String256 m_deviceName;

	ID3D12RootSignature* m_graphicsRootSig = nullptr;
	ID3D12RootSignature* m_computeRootSig = nullptr;

	ID3D12Device2* m_d3dDev = nullptr;
	IDXGISwapChain3* m_swapChain = nullptr;
	
	gpu::TextureRef m_backBuffers[c_maxBufferedFrames];

	gpu::TextureRef m_backbufferDepth;

	CommandQueueManager_D3D12 m_commandQueueManager;

	FreeListDescriptorHeap_D3D12 m_rtvHeap;
	FreeListDescriptorHeap_D3D12 m_dsvHeap;
	FreeListDescriptorHeap_D3D12 m_stagingHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE m_nullCbv;
	D3D12_CPU_DESCRIPTOR_HANDLE m_nullSrv;
	D3D12_CPU_DESCRIPTOR_HANDLE m_nullUav;

	D3D12_GPU_DESCRIPTOR_HANDLE m_nullCbvTable;
	D3D12_GPU_DESCRIPTOR_HANDLE m_nullSrvTable;
	D3D12_GPU_DESCRIPTOR_HANDLE m_nullUavTable;

	FrameUploadPagePool_D3D12 m_uploadPagePool;

	FrameResources m_framesResources[c_maxBufferedFrames];

	DescriptorHeap_D3D12 m_cbvsrvuavHeap;
	RingBufferDescriptorHeap_D3D12 m_descriptorcbvsrvuavRingBuffer;

	kt::VersionedHandlePool<AllocatedResource_D3D12>	m_resourceHandles;
	kt::VersionedHandlePool<AllocatedShader_D3D12>		m_shaderHandles;
	kt::VersionedHandlePool<AllocatedPSO_D3D12>			m_psoHandles;

	cmd::CommandContext_D3D12* m_mainThreadCtx = nullptr;

	PSOCache m_psoCache;

	struct MipPSOs
	{
		// Second is srgb
		gpu::PSORef m_standard[2];
		gpu::PSORef m_cube[2];
		gpu::PSORef m_array[2];
	} m_mipPsos;

	uint64_t m_frameFences[c_maxBufferedFrames] = {};

	uint32_t m_cpuFrameIdx = 0;

	uint32_t m_swapChainWidth = 0;
	uint32_t m_swapChainHeight = 0;

	uint32_t m_frameCounter = 0;

	bool m_withDebugLayer = false;
	bool m_vsync = false;

private:
	void InitDescriptorHeaps();
};

}