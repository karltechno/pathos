#include "GPUDevice_D3D12.h"
#include "Utils_D3D12.h"
#include "Types.h"
#include "CommandContext_D3D12.h"

#include <kt/Macros.h>
#include <kt/Logging.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include <kt/Hash.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include "d3dx12.h"

#include <string.h>

#include "GenMipsArrayLinear.h"
#include "GenMipsArraySRGB.h"
#include "GenMipsCubeLinear.h"
#include "GenMipsCubeSRGB.h"
#include "GenMipsLinear.h"
#include "GenMipsSRGB.h"

static D3D12_HEAP_PROPERTIES const c_defaultHeapProperties{ D3D12_HEAP_TYPE_DEFAULT , D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
static D3D12_HEAP_PROPERTIES const c_uploadHeapProperties{ D3D12_HEAP_TYPE_UPLOAD , D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

namespace gpu
{

Device_D3D12* g_device = nullptr;

// For buffers that will only be in an upload heap.
static uint32_t RequiredUploadHeapAlign(AllocatedResource_D3D12 const& _res)
{
	if (_res.IsBuffer())
	{
		if (!!(_res.m_bufferDesc.m_flags & gpu::BufferFlags::Constant))
		{
			return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		}
		else
		{
			return 16; // Anything better ?
		}
	}
	else
	{
		return D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
	}

}

static void CopyInitialResourceData(ID3D12Resource* _resource, void const* _data, uint32_t _size, D3D12_RESOURCE_STATES _beforeState, D3D12_RESOURCE_STATES _afterState)
{
	ScratchAlloc_D3D12 scratchMem = g_device->GetFrameResources()->m_uploadAllocator.Alloc(_size, 16); // TODO: Any other align needed?
	memcpy(scratchMem.m_cpuData, _data, _size);

	ID3D12CommandAllocator* allocator = g_device->m_commandQueueManager.GraphicsQueue().AcquireAllocator();
	// TODO: Pool lists
	ID3D12CommandList* listBase;
	// TODO: Use direct queue for now, need to work out synchronization and use copy queue.
	D3D_CHECK(g_device->m_d3dDev->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&listBase)));
	ID3D12GraphicsCommandList* list = (ID3D12GraphicsCommandList*)listBase;
	list->CopyBufferRegion(_resource, 0, scratchMem.m_res, scratchMem.m_offset, _size);
	
	if (_beforeState != _afterState)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = _resource;
		barrier.Transition.StateBefore = _beforeState;
		barrier.Transition.StateAfter = _afterState;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		list->ResourceBarrier(1, &barrier);
	}

	uint64_t const fence = g_device->m_commandQueueManager.GraphicsQueue().ExecuteCommandLists(kt::MakeSlice(listBase));
	g_device->m_commandQueueManager.GraphicsQueue().ReleaseAllocator(allocator, fence);
	listBase->Release();
}

static void CopyInitialTextureData(AllocatedResource_D3D12& _tex, void const* _data, D3D12_RESOURCE_STATES _beforeState, D3D12_RESOURCE_STATES _afterState)
{
	// TODO: Texture arrays.
	// TODO: Cubemaps.
	// TODO: Assert size of input data?
	// TODO: Handle pitch more robustly?

	UINT64 totalBytes;
	D3D12_RESOURCE_DESC const d3dDesc = _tex.m_res->GetDesc();
	uint32_t const numSubresources = d3dDesc.MipLevels;
	g_device->m_d3dDev->GetCopyableFootprints(&d3dDesc, 0, numSubresources, 0, nullptr, nullptr, nullptr, &totalBytes);
	KT_ASSERT(totalBytes);
	ScratchAlloc_D3D12 uploadScratch = g_device->GetFrameResources()->m_uploadAllocator.Alloc(uint32_t(totalBytes), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

	ID3D12CommandAllocator* allocator = g_device->m_commandQueueManager.GraphicsQueue().AcquireAllocator();
	// TODO: Pool lists
	ID3D12CommandList* listBase;
	// TODO: Use direct queue for now, need to work out synchronization and use copy queue.
	D3D_CHECK(g_device->m_d3dDev->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&listBase)));
	ID3D12GraphicsCommandList* list = (ID3D12GraphicsCommandList*)listBase;

	uint32_t const bpp = gpu::GetFormatSize(_tex.m_textureDesc.m_format);
	D3D12_SUBRESOURCE_DATA* srcData = (D3D12_SUBRESOURCE_DATA*)KT_ALLOCA(sizeof(D3D12_SUBRESOURCE_DATA) * numSubresources);
	
	uint32_t mipWidth = uint32_t(d3dDesc.Width);
	uint32_t mipHeight = uint32_t(d3dDesc.Height);

	uint8_t* pData = (uint8_t*)_data;

	for (uint32_t i = 0; i < numSubresources; ++i)
	{
		srcData[i].pData = pData;
		srcData[i].RowPitch = bpp * mipWidth;
		srcData[i].SlicePitch = bpp * mipWidth * mipHeight;
		pData += bpp * mipWidth * mipHeight;

		mipWidth = kt::Max<uint32_t>(1, mipWidth >> 1);
		mipHeight = kt::Max<uint32_t>(1, mipHeight >> 1);
	}

	UpdateSubresources<16>(list, _tex.m_res, uploadScratch.m_res, uploadScratch.m_offset, 0, d3dDesc.MipLevels, srcData);

	if (_beforeState != _afterState)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = _tex.m_res;
		barrier.Transition.StateBefore = _beforeState;
		barrier.Transition.StateAfter = _afterState;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		list->ResourceBarrier(1, &barrier);
	}

	uint64_t const fence = g_device->m_commandQueueManager.GraphicsQueue().ExecuteCommandLists(kt::MakeSlice(listBase));
	g_device->m_commandQueueManager.GraphicsQueue().ReleaseAllocator(allocator, fence);
	listBase->Release();
}

bool AllocatedResource_D3D12::InitAsBuffer(BufferDesc const& _desc, void const* _initialData, char const* _debugName)
{
	KT_ASSERT(!m_res);

	m_bufferDesc = _desc;
	m_type = ResourceType::Buffer;

	uint32_t const initialDataSize = _desc.m_sizeInBytes;

	// constant buffers need to be a multiple of 256.
	if (!!(m_bufferDesc.m_flags & gpu::BufferFlags::Constant))
	{
		m_bufferDesc.m_sizeInBytes = uint32_t(kt::AlignUp(m_bufferDesc.m_sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
	}

	gpu::ResourceState bestInitialState = gpu::ResourceState::Common;
	if (!!(m_bufferDesc.m_flags & BufferFlags::Constant))
	{
		bestInitialState = gpu::ResourceState::ConstantBuffer;
	}
	else if (!!(m_bufferDesc.m_flags & BufferFlags::Vertex))
	{
		bestInitialState = gpu::ResourceState::VertexBuffer;
	}
	else if (!!(m_bufferDesc.m_flags & BufferFlags::Index))
	{
		bestInitialState = gpu::ResourceState::IndexBuffer;
	}
	else if (!!(m_bufferDesc.m_flags & BufferFlags::ShaderResource))
	{
		bestInitialState = gpu::ResourceState::ShaderResource;
	}
	else if (!!(m_bufferDesc.m_flags & BufferFlags::UnorderedAccess))
	{
		bestInitialState = gpu::ResourceState::UnorderedAccess;
	}

	gpu::ResourceState const creationState = _initialData ? gpu::ResourceState::CopyDest : bestInitialState;
	m_resState = creationState;


	if (!!(m_bufferDesc.m_flags & BufferFlags::Constant))
	{
		m_cbv = g_device->m_stagingHeap.AllocOne();
	}

	if (!!(m_bufferDesc.m_flags & BufferFlags::UnorderedAccess))
	{
		m_uavs.PushBack(g_device->m_stagingHeap.AllocOne());
	}

	if (!(m_bufferDesc.m_flags & BufferFlags::Transient))
	{
		m_ownsResource = true;
		// Create a committed buffer if not transient. 
		// TODO: Make an actual heap allocator.

		D3D12_RESOURCE_DESC desc{};
		desc.Alignment = 0;
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Flags = !!(m_bufferDesc.m_flags & BufferFlags::UnorderedAccess) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Height = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Width = m_bufferDesc.m_sizeInBytes;

		if (!SUCCEEDED(g_device->m_d3dDev->CreateCommittedResource(&c_defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, gpu::D3DTranslateResourceState(creationState), nullptr, IID_PPV_ARGS(&m_res))))
		{
			KT_LOG_ERROR("CreateCommittedResource failed to create buffer size %u.", m_bufferDesc.m_sizeInBytes);
			return false;
		}

		m_gpuAddress = m_res->GetGPUVirtualAddress();
		m_offset = 0;

		UpdateViews();
	}
	else
	{
		KT_ASSERT(!(m_bufferDesc.m_flags & BufferFlags::Dynamic) && "Can't be transient and dynamic!");
	}

	AllocatedObjectBase_D3D12::Init(_debugName);

	if (m_res)
	{
		D3D_SET_DEBUG_NAME(m_res, m_debugName.Data());
	}

	if (_initialData)
	{
		if (!!(_desc.m_flags & BufferFlags::Transient))
		{
			// Transient with initial data.
			g_device->GetFrameResources()->m_uploadAllocator.Alloc(*this);
			m_lastFrameTouched = g_device->m_frameCounter;
			KT_ASSERT(m_mappedCpuData);
			memcpy(m_mappedCpuData, _initialData, initialDataSize);
			UpdateViews();
		}
		else
		{
			KT_ASSERT(m_res);
			CopyInitialResourceData(m_res, _initialData, initialDataSize, gpu::D3DTranslateResourceState(creationState), gpu::D3DTranslateResourceState(bestInitialState));
			m_resState = bestInitialState;
		}
	}

	return true;
}

void AllocatedResource_D3D12::Destroy()
{
	if (m_ownsResource && m_res)
	{
		g_device->GetFrameResources()->m_deferredDeletions.PushBack(m_res);
	}

	m_res = nullptr;

	if (m_srv.ptr)
	{
		g_device->m_stagingHeap.Free(m_srv);
		m_srv.ptr = 0;
	}

	if (m_srvCubeAsArray.ptr)
	{
		g_device->m_stagingHeap.Free(m_srvCubeAsArray);
		m_srv.ptr = 0;
	}

	for (D3D12_CPU_DESCRIPTOR_HANDLE uav : m_uavs)
	{
		g_device->m_stagingHeap.Free(uav);
	}
	m_uavs.Clear();

	if (m_cbv.ptr)
	{
		g_device->m_stagingHeap.Free(m_cbv);
		m_cbv.ptr = 0;
	}

	if (m_rtv.ptr)
	{
		g_device->m_rtvHeap.Free(m_rtv);
		m_rtv.ptr = 0;
	}

	if (m_dsv.ptr)
	{
		g_device->m_dsvHeap.Free(m_dsv);
		m_dsv.ptr = 0;
	}

	m_mappedCpuData = nullptr;
	m_gpuAddress = 0;
	m_lastFrameTouched = 0xFFFFFFFF;
	m_type = ResourceType::Num_ResourceType;
}


void AllocatedResource_D3D12::UpdateViews()
{
	if (!!(m_bufferDesc.m_flags & BufferFlags::Constant))
	{
		KT_ASSERT(m_cbv.ptr);
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
		cbvDesc.BufferLocation = m_gpuAddress;
		cbvDesc.SizeInBytes = m_bufferDesc.m_sizeInBytes;
		g_device->m_d3dDev->CreateConstantBufferView(&cbvDesc, m_cbv);
	}

	if (!!(m_bufferDesc.m_flags & BufferFlags::ShaderResource))
	{
		KT_ASSERT(m_srv.ptr);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = ToDXGIFormat(m_bufferDesc.m_format);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = m_bufferDesc.m_strideInBytes / m_bufferDesc.m_sizeInBytes;
		srvDesc.Buffer.StructureByteStride = m_bufferDesc.m_strideInBytes;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		g_device->m_d3dDev->CreateShaderResourceView(m_res, &srvDesc, m_srv);
	}
}

void AllocatedResource_D3D12::UpdateTransientSize(uint32_t _size)
{
	KT_ASSERT(!!(m_bufferDesc.m_flags & gpu::BufferFlags::Transient));
	if (!!(m_bufferDesc.m_flags & gpu::BufferFlags::Constant))
	{
		m_bufferDesc.m_sizeInBytes = uint32_t(kt::AlignUp(_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
	}
	else
	{
		m_bufferDesc.m_sizeInBytes = _size;
	}
}

bool AllocatedResource_D3D12::IsBuffer() const
{
	return m_type == ResourceType::Buffer;
}

bool AllocatedResource_D3D12::IsTexture() const
{
	KT_ASSERT(m_type != ResourceType::Num_ResourceType);
	return !IsBuffer();
}

struct AllocatedShader_D3D12 : AllocatedObjectBase_D3D12
{
	void Init(ShaderType _type, ShaderBytecode const& _byteCode, char const* _name = nullptr)
	{
		m_shaderType = _type;
		CopyBytecode(_byteCode);

		AllocatedObjectBase_D3D12::Init(_name);
	}

	void Destroy()
	{
		FreeBytecode();
		m_linkedPsos.Clear();
	}

	void FreeBytecode()
	{
		kt::GetDefaultAllocator()->FreeSized((void*)m_byteCode.m_data, m_byteCode.m_size);
		m_byteCode = ShaderBytecode{};
	}

	void CopyBytecode(ShaderBytecode const& _byteCode)
	{
		KT_ASSERT(!m_byteCode.m_data);
		m_byteCode.m_data = kt::GetDefaultAllocator()->Alloc(_byteCode.m_size);
		m_byteCode.m_size = _byteCode.m_size;
		memcpy((void*)m_byteCode.m_data, _byteCode.m_data, _byteCode.m_size);
	}

	ShaderBytecode m_byteCode;
	ShaderType m_shaderType;
	
	kt::InplaceArray<PSOHandle, 4> m_linkedPsos;
};

static ID3D12PipelineState* CreateD3DComputePSO(ID3D12Device* _device, gpu::ShaderBytecode const& _cs)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
	desc.CS = D3D12_SHADER_BYTECODE{ _cs.m_data, _cs.m_size };
	desc.pRootSignature = g_device->m_computeRootSig;
	ID3D12PipelineState* state;
	D3D_CHECK(_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&state)));
	return state;
}


static ID3D12PipelineState* CreateD3DGraphicsPSO(ID3D12Device* _device, gpu::GraphicsPSODesc const& _desc, gpu::ShaderBytecode const& _vs, gpu::ShaderBytecode const& _ps)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3dDesc{};

	d3dDesc.VS = D3D12_SHADER_BYTECODE{ _vs.m_data, _vs.m_size };
	d3dDesc.PS = D3D12_SHADER_BYTECODE{ _ps.m_data, _ps.m_size };

	d3dDesc.pRootSignature = g_device->m_graphicsRootSig;

	d3dDesc.BlendState.AlphaToCoverageEnable = _desc.m_blendDesc.m_alphaToCoverageEnable;
	d3dDesc.BlendState.IndependentBlendEnable = FALSE;

	d3dDesc.BlendState.RenderTarget[0].BlendEnable = _desc.m_blendDesc.m_blendEnable;
	d3dDesc.BlendState.RenderTarget[0].BlendOp = ToD3DBlendOP(_desc.m_blendDesc.m_blendOp);
	d3dDesc.BlendState.RenderTarget[0].BlendOpAlpha = ToD3DBlendOP(_desc.m_blendDesc.m_blendOpAlpha);
	d3dDesc.BlendState.RenderTarget[0].DestBlend = ToD3DBlend(_desc.m_blendDesc.m_destBlend);
	d3dDesc.BlendState.RenderTarget[0].DestBlendAlpha = ToD3DBlend(_desc.m_blendDesc.m_destAlpha);
	d3dDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	d3dDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // expose
	d3dDesc.BlendState.RenderTarget[0].SrcBlend = ToD3DBlend(_desc.m_blendDesc.m_srcBlend);
	d3dDesc.BlendState.RenderTarget[0].SrcBlendAlpha = ToD3DBlend(_desc.m_blendDesc.m_srcAlpha);

	static D3D12_DEPTH_STENCILOP_DESC const s_defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };

	d3dDesc.DepthStencilState.BackFace = s_defaultStencilOp;
	d3dDesc.DepthStencilState.FrontFace = s_defaultStencilOp;
	d3dDesc.DepthStencilState.DepthEnable = _desc.m_depthStencilDesc.m_depthEnable;
	d3dDesc.DepthStencilState.DepthFunc = ToD3DCmpFn(_desc.m_depthStencilDesc.m_depthFn);
	d3dDesc.DepthStencilState.DepthWriteMask = _desc.m_depthStencilDesc.m_depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	d3dDesc.DepthStencilState.StencilEnable = FALSE;
	d3dDesc.DepthStencilState.StencilReadMask = 0xFF;
	d3dDesc.DepthStencilState.StencilWriteMask = 0xFF;

	d3dDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	d3dDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	CullMode const cullMode = _desc.m_rasterDesc.m_cullMode;
	d3dDesc.RasterizerState.CullMode = cullMode == CullMode::Back ? D3D12_CULL_MODE_BACK : (cullMode == CullMode::Front ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_NONE);
	d3dDesc.RasterizerState.FillMode = _desc.m_rasterDesc.m_fillMode == FillMode::Solid ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
	d3dDesc.RasterizerState.FrontCounterClockwise = _desc.m_rasterDesc.m_frontFaceCCW ? TRUE : FALSE;
	d3dDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	d3dDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	d3dDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	d3dDesc.RasterizerState.DepthClipEnable = TRUE;
	d3dDesc.RasterizerState.MultisampleEnable = FALSE;
	d3dDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	d3dDesc.RasterizerState.ForcedSampleCount = 0;
	d3dDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	d3dDesc.DSVFormat = ToDXGIFormat(_desc.m_depthFormat);

	d3dDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	d3dDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // TODO: Specify this.

	d3dDesc.NumRenderTargets = _desc.m_numRenderTargets;
	for (uint32_t i = 0; i < _desc.m_numRenderTargets; ++i)
	{
		d3dDesc.RTVFormats[i] = ToDXGIFormat(_desc.m_renderTargetFormats[i]);
	}

	d3dDesc.SampleDesc.Count = 1; // TOOD: MSAA
	d3dDesc.SampleDesc.Quality = 0;
	d3dDesc.SampleMask = 0xFFFFFFFF;

	D3D12_INPUT_ELEMENT_DESC inputElements[c_maxVertexElements];
	d3dDesc.InputLayout.pInputElementDescs = inputElements;
	d3dDesc.InputLayout.NumElements = _desc.m_vertexLayout.m_numElements;

	for (uint32_t i = 0; i < _desc.m_vertexLayout.m_numElements; ++i)
	{
		VertexDeclEntry const& entry = _desc.m_vertexLayout.m_elements[i];

		inputElements[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT; // TODO: Allow non append
		inputElements[i].Format = ToDXGIFormat(entry.m_format);
		inputElements[i].InputSlot = entry.m_streamIdx;
		inputElements[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		inputElements[i].SemanticIndex = entry.m_semanticIndex;
		inputElements[i].SemanticName = ToD3DSemanticStr(entry.m_semantic);
		inputElements[i].InstanceDataStepRate = 0;
	}

	ID3D12PipelineState* pso;
	D3D_CHECK(_device->CreateGraphicsPipelineState(&d3dDesc, IID_PPV_ARGS(&pso)));
	return pso;
}

void AllocatedPSO_D3D12::InitAsCompute(gpu::ShaderHandle _handle, gpu::ShaderBytecode const _byteCode, char const* _debugName)
{
	KT_ASSERT(!m_pso);
	gpu::AddRef(_handle);
	m_cs = _handle;
	m_pso = CreateD3DComputePSO(g_device->m_d3dDev, _byteCode);

	AllocatedObjectBase_D3D12::Init(_debugName);
	D3D_SET_DEBUG_NAME(m_pso, m_debugName.Data());
}

void AllocatedPSO_D3D12::InitAsGraphics
(
	gpu::GraphicsPSODesc const& _desc, 
	gpu::ShaderBytecode const& _vs,  
	gpu::ShaderBytecode const& _ps,
	char const* _debugName
)
{
	KT_ASSERT(!m_pso);
	m_psoDesc = _desc;
	gpu::AddRef(m_psoDesc.m_vs);
	gpu::AddRef(m_psoDesc.m_ps);
	
	m_pso = CreateD3DGraphicsPSO(g_device->m_d3dDev, _desc, _vs, _ps);

	AllocatedObjectBase_D3D12::Init(_debugName);
	D3D_SET_DEBUG_NAME(m_pso, m_debugName.Data());
}



bool AllocatedResource_D3D12::InitAsTexture(TextureDesc const& _desc, void const* _initialData, char const* _debugName /* = nullptr */)
{
	KT_ASSERT(!m_res);
	m_textureDesc = _desc;
	m_type = _desc.m_type;
	m_ownsResource = true;

	gpu::ResourceState bestInitialState = gpu::ResourceState::Common;

	// TODO: Something better?
	if (!!(_desc.m_usageFlags & TextureUsageFlags::DepthStencil))
	{
		bestInitialState = gpu::ResourceState::DepthStencilTarget;
	}
	else if (!!(_desc.m_usageFlags & TextureUsageFlags::RenderTarget))
	{
		bestInitialState = gpu::ResourceState::RenderTarget;
	}
	else if (!!(_desc.m_usageFlags & TextureUsageFlags::ShaderResource))
	{
		bestInitialState = gpu::ResourceState::ShaderResource;
	}
	else if (!!(_desc.m_usageFlags & TextureUsageFlags::UnorderedAccess))
	{
		bestInitialState = gpu::ResourceState::UnorderedAccess;
	}

	gpu::ResourceState const creationState = _initialData ? gpu::ResourceState::CopyDest : bestInitialState;
	m_resState = creationState;

	D3D12_RESOURCE_DESC d3dDesc = {};

	if (!(_desc.m_usageFlags & gpu::TextureUsageFlags::ShaderResource))
	{
		d3dDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	if (!!(_desc.m_usageFlags & gpu::TextureUsageFlags::UnorderedAccess))
	{
		d3dDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if (!!(_desc.m_usageFlags & gpu::TextureUsageFlags::DepthStencil))
	{
		d3dDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}

	if (!!(_desc.m_usageFlags & gpu::TextureUsageFlags::RenderTarget))
	{
		d3dDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	d3dDesc.Width = _desc.m_width;
	d3dDesc.Height = _desc.m_height;
	d3dDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dDesc.MipLevels = UINT16(_desc.m_mipLevels);
	d3dDesc.Format = ToDXGIFormat(_desc.m_format);
	d3dDesc.Alignment = 0;
	d3dDesc.DepthOrArraySize = UINT16(_desc.m_arraySlices);

	// TODO: MSAA
	d3dDesc.SampleDesc.Count = 1;
	d3dDesc.SampleDesc.Quality = 0;

	switch (_desc.m_type)
	{
		case ResourceType::Texture1D:
		{
			d3dDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			d3dDesc.Width = _desc.m_width;
		} break;

		case ResourceType::Texture2D:
		{
			d3dDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			d3dDesc.Width = _desc.m_width;
			d3dDesc.Height = _desc.m_height;
		} break;

		case ResourceType::Texture3D:
		{
			d3dDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			d3dDesc.Width = _desc.m_width;
			d3dDesc.Height = _desc.m_height;
			d3dDesc.DepthOrArraySize = UINT16(_desc.m_depth);
		} break;

		case ResourceType::TextureCube:
		{
			d3dDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			d3dDesc.Width = _desc.m_width;
			d3dDesc.Height = _desc.m_height;
			d3dDesc.DepthOrArraySize = UINT16(6 * _desc.m_arraySlices);
		} break;

		default:
		{
			KT_ASSERT(!"Bad TextureType");
			return false;
		} break;
	}

	D3D12_CLEAR_VALUE depthClear;
	D3D12_CLEAR_VALUE* pClearVal = nullptr;
	if (gpu::IsDepthFormat(_desc.m_format))
	{
		pClearVal = &depthClear;
		depthClear.Format = ToDXGIFormat(_desc.m_format);
		depthClear.DepthStencil.Stencil = 0;
		depthClear.DepthStencil.Depth = 1.0f; // TODO: Selectable for reverse Z
	}

	HRESULT const hr = g_device->m_d3dDev->CreateCommittedResource
	(
		&c_defaultHeapProperties, 
		D3D12_HEAP_FLAG_NONE, 
		&d3dDesc,
		gpu::D3DTranslateResourceState(creationState), 
		pClearVal, 
		IID_PPV_ARGS(&m_res)
	);

	if (!SUCCEEDED(hr))
	{
		KT_LOG_ERROR("CreateCommittedResource failed (HRESULT: %u)", hr);
		KT_ASSERT(false);
		return false;
	}

	m_gpuAddress = 0;

	if (!!(_desc.m_usageFlags & gpu::TextureUsageFlags::DepthStencil))
	{
		m_dsv = g_device->m_dsvHeap.AllocOne();
		g_device->m_d3dDev->CreateDepthStencilView(m_res, nullptr, m_dsv);
	}

	if (!!(_desc.m_usageFlags & gpu::TextureUsageFlags::RenderTarget))
	{
		m_rtv = g_device->m_rtvHeap.AllocOne();
		g_device->m_d3dDev->CreateRenderTargetView(m_res, nullptr, m_rtv);
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	// TODO: Might need to convert uav fmt
	uavDesc.Format = d3dDesc.Format;

	srvDesc.Format = d3dDesc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	switch (d3dDesc.Dimension)
	{
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		{
			srvDesc.ViewDimension = d3dDesc.DepthOrArraySize == 1 ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			if (d3dDesc.DepthOrArraySize == 1)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

				srvDesc.Texture2D.MipLevels = d3dDesc.MipLevels;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

				uavDesc.Texture2D.PlaneSlice = 0;
				uavDesc.Texture2D.MipSlice = 0;
			}
			else
			{
				// texture 2d array (or cube)
				if (_desc.m_type == ResourceType::TextureCube)
				{
					KT_ASSERT(d3dDesc.DepthOrArraySize % 6 == 0);
					if (d3dDesc.DepthOrArraySize > 6)
					{
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
						srvDesc.TextureCubeArray.First2DArrayFace = 0;
						srvDesc.TextureCubeArray.MipLevels = d3dDesc.MipLevels;
						srvDesc.TextureCubeArray.MostDetailedMip = 0;
						srvDesc.TextureCubeArray.NumCubes = d3dDesc.DepthOrArraySize / 6;
						srvDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
					}
					else
					{
						srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
						srvDesc.TextureCube.MipLevels = d3dDesc.MipLevels;
						srvDesc.TextureCube.MostDetailedMip = 0;
						srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
					}
				}
				else
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					srvDesc.Texture2DArray.ArraySize = d3dDesc.DepthOrArraySize;
					srvDesc.Texture2DArray.FirstArraySlice = 0;
					srvDesc.Texture2DArray.MostDetailedMip = 0;
					srvDesc.Texture2DArray.PlaneSlice = 0;
					srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
				}

				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.ArraySize = d3dDesc.DepthOrArraySize;
				uavDesc.Texture2DArray.FirstArraySlice = 0;
				uavDesc.Texture2DArray.MipSlice = 0;
				uavDesc.Texture2DArray.PlaneSlice = 0;
			}
		} break;
		
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		{
			// TODO;
			KT_ASSERT(false);
			KT_UNREACHABLE;
		} break;

		default: KT_ASSERT(false); KT_UNREACHABLE;
	}

	if (!!(_desc.m_usageFlags & gpu::TextureUsageFlags::ShaderResource))
	{
		m_srv = g_device->m_stagingHeap.AllocOne();
		g_device->m_d3dDev->CreateShaderResourceView(m_res, &srvDesc, m_srv);

		if (_desc.m_type == ResourceType::TextureCube)
		{
			m_srvCubeAsArray = g_device->m_stagingHeap.AllocOne();
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.ArraySize = 6 * _desc.m_arraySlices;
			srvDesc.Texture2DArray.FirstArraySlice = 0;
			srvDesc.Texture2DArray.MipLevels = _desc.m_mipLevels;
			srvDesc.Texture2DArray.MostDetailedMip = 0;
			srvDesc.Texture2DArray.PlaneSlice = 0;
			srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
			g_device->m_d3dDev->CreateShaderResourceView(m_res, &srvDesc, m_srvCubeAsArray);
		}
	}

	if (!!(_desc.m_usageFlags & gpu::TextureUsageFlags::UnorderedAccess))
	{
		D3D12_CPU_DESCRIPTOR_HANDLE* uavs = m_uavs.PushBack_Raw(d3dDesc.MipLevels);

		for (uint32_t i = 0; i < d3dDesc.MipLevels; ++i)
		{
			uavs[i] = g_device->m_stagingHeap.AllocOne();
			// mipslice is first in all union members
			uavDesc.Texture1D.MipSlice = i;
			g_device->m_d3dDev->CreateUnorderedAccessView(m_res, nullptr, &uavDesc, uavs[i]);
		}
	}

	AllocatedObjectBase_D3D12::Init(_debugName);
	if (m_res)
	{
		D3D_SET_DEBUG_NAME(m_res, m_debugName.Data());
	}

	if (_initialData)
	{
		D3D12_RESOURCE_STATES const destState = gpu::D3DTranslateResourceState(bestInitialState);
		CopyInitialTextureData(*this, _initialData, gpu::D3DTranslateResourceState(creationState), destState);
		m_resState = bestInitialState;
	}

	return true;
}

void AllocatedResource_D3D12::InitFromBackbuffer(ID3D12Resource* _res, uint32_t _idx, gpu::Format _format, uint32_t _height, uint32_t _width)
{
	m_type = ResourceType::Texture2D;

	m_res = _res;
	m_gpuAddress = 0;
	m_ownsResource = true;
	
	m_textureDesc = TextureDesc::Desc2D(_width, _height, TextureUsageFlags::RenderTarget, _format);

	// https://docs.microsoft.com/en-us/windows/desktop/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#initial-states-for-resources
	// "Swap chain back buffers automatically start out in the D3D12_RESOURCE_STATE_COMMON state."
	// Note: present resource state is the same as common in d3d12.
	m_resState = gpu::ResourceState::Common;

	m_rtv = g_device->m_rtvHeap.AllocOne();
	m_srv = g_device->m_stagingHeap.AllocOne();
	g_device->m_d3dDev->CreateRenderTargetView(m_res, nullptr, m_rtv);
	g_device->m_d3dDev->CreateShaderResourceView(m_res, nullptr, m_srv);

	kt::String64 name;
	name.AppendFmt("Backbuffer %u", _idx);
	AllocatedObjectBase_D3D12::Init(name.Data());

	if (m_res)
	{
		D3D_SET_DEBUG_NAME(m_res, m_debugName.Data());
	}
}

void AllocatedPSO_D3D12::Destroy()
{
	if (m_pso)
	{
		g_device->GetFrameResources()->m_deferredDeletions.PushBack(m_pso);
		m_pso = nullptr;
	}

	if (m_psoDesc.m_ps.IsValid())
	{
		gpu::Release(m_psoDesc.m_ps);
		m_psoDesc.m_ps = gpu::ShaderHandle{};
	}

	if (m_psoDesc.m_vs.IsValid())
	{
		gpu::Release(m_psoDesc.m_vs);
		m_psoDesc.m_vs = gpu::ShaderHandle{};
	}

	if (m_cs.IsValid())
	{
		gpu::Release(m_cs);
		m_cs = gpu::ShaderHandle{};
	}

	m_psoDesc = gpu::GraphicsPSODesc{};
}


void FrameUploadPagePool_D3D12::Init(ID3D12Device* _device)
{
	m_device = _device;
}

void FrameUploadPagePool_D3D12::Shutdown()
{
	for (FrameUploadPage_D3D12& page : m_freePages)
	{
		page.m_res->Release();
	}

	m_freePages.Clear();
}

gpu::FrameUploadPage_D3D12 FrameUploadPagePool_D3D12::AllocPage(uint32_t _minSize)
{
	FrameUploadPage_D3D12 page;
	for (uint32_t i = 0; i < m_freePages.Size(); ++i)
	{
		if (m_freePages[i].SizeLeft() >= _minSize)
		{
			page = m_freePages[i];
			m_freePages.EraseSwap(i);
			return page;
		}
	}

	// Create a new page.

	D3D12_RESOURCE_DESC resDesc{};
	uint32_t const resSize = kt::Max(_minSize, c_defaultPageSize);

	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.Width = resSize;
	resDesc.Alignment = 0;
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Height = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;

	D3D_CHECK(m_device->CreateCommittedResource(&c_uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&page.m_res)));
	page.m_base = page.m_res->GetGPUVirtualAddress();
	page.m_curOffset = 0;
	page.m_size = resSize;
	D3D12_RANGE const readRange{ 0, 0 };
	D3D_CHECK(page.m_res->Map(0, &readRange, &page.m_mappedPtr));

	return page;
}

void FrameUploadPagePool_D3D12::ReleasePage(FrameUploadPage_D3D12 const& _page)
{
#if 1
	FrameUploadPage_D3D12& page = m_freePages.PushBack(_page);
	page.m_curOffset = 0;
#else
	_page.m_res->Release();
#endif
}

void FrameUploadAllocator_D3D12::Init(FrameUploadPagePool_D3D12* _pagePool)
{
	m_pagePool = _pagePool;
}

void FrameUploadAllocator_D3D12::Shutdown()
{
	for (FrameUploadPage_D3D12& page : m_pages)
	{
		page.m_res->Release();
	}
	m_pages.Clear();
}


void FrameUploadAllocator_D3D12::ClearOnBeginFrame()
{
	for (FrameUploadPage_D3D12& page : m_pages)
	{
		m_pagePool->ReleasePage(page);
	}
	m_pages.Clear();
	m_numFullPages = 0;
}

ScratchAlloc_D3D12 FrameUploadAllocator_D3D12::Alloc(uint32_t _size, uint32_t _align)
{
	do 
	{
		KT_ASSERT(m_numFullPages <= m_pages.Size());
		FrameUploadPage_D3D12* page = nullptr;
		if (m_pages.Size() == m_numFullPages)
		{
			page = &m_pages.PushBack(m_pagePool->AllocPage(_size));
		}
		else
		{
			KT_ASSERT(m_numFullPages == m_pages.Size() - 1);
			page = &m_pages.Back();
		}

		uintptr_t const alignedAddr = kt::AlignUp(page->m_base + page->m_curOffset, _align);
		uintptr_t const endAddr = alignedAddr + _size;
		if (endAddr > page->m_base + page->m_size)
		{
			// TODO: A lot of potential wastage here if we do a large alloc
			++m_numFullPages;
			continue;
		}

		
		ScratchAlloc_D3D12 alloc;

		alloc.m_offset = alignedAddr - page->m_base;
		alloc.m_addr = page->m_base + alloc.m_offset;
		alloc.m_res = page->m_res;
		alloc.m_cpuData = (uint8_t*)page->m_mappedPtr + alloc.m_offset;

		KT_ASSERT(alloc.m_addr >= page->m_base + page->m_curOffset);
		KT_ASSERT(alloc.m_offset >= page->m_curOffset);

		page->m_curOffset = uint32_t(endAddr - page->m_base);


		return alloc;

	} while (true);

	KT_UNREACHABLE;
}

void FrameUploadAllocator_D3D12::Alloc(AllocatedResource_D3D12& o_res)
{
	ScratchAlloc_D3D12 scratch = Alloc(o_res.m_bufferDesc.m_sizeInBytes, RequiredUploadHeapAlign(o_res));
	o_res.m_mappedCpuData = scratch.m_cpuData;
	o_res.m_res = scratch.m_res;
	o_res.m_offset = scratch.m_offset;
	o_res.m_gpuAddress = scratch.m_addr;

	o_res.m_ownsResource = false;
	o_res.m_resState = ResourceState::Unknown; // GENERIC_READ, but no code can ever transition out of it! (memory is in upload heap)
}

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


static void CreateRootSigs(ID3D12Device* _dev, ID3D12RootSignature*& o_gfx, ID3D12RootSignature*& o_compute)
{
	D3D12_ROOT_SIGNATURE_DESC graphicsDesc = {};
	D3D12_ROOT_SIGNATURE_DESC computeDesc = {};

	D3D12_STATIC_SAMPLER_DESC samplers[5] = {};

	graphicsDesc.pStaticSamplers = samplers;
	graphicsDesc.NumStaticSamplers = KT_ARRAY_COUNT(samplers);

	computeDesc.pStaticSamplers = samplers;
	computeDesc.NumStaticSamplers = KT_ARRAY_COUNT(samplers);

	graphicsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	computeDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// Point clamp
	{
		samplers[0].AddressU = samplers[0].AddressV = samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplers[0].MinLOD = 0.0f;
		samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
		samplers[0].MipLODBias = 0.0f;
		samplers[0].RegisterSpace = 0;
		samplers[0].ShaderRegister = 0;
		samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	// Point Wrap
	{
		samplers[1].AddressU = samplers[1].AddressV = samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplers[1].MinLOD = 0.0f;
		samplers[1].MaxLOD = D3D12_FLOAT32_MAX;
		samplers[1].MipLODBias = 0.0f;
		samplers[1].RegisterSpace = 0;
		samplers[1].ShaderRegister = 1;
		samplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	// Linear clamp
	{
		samplers[2].AddressU = samplers[2].AddressV = samplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplers[2].MinLOD = 0.0f;
		samplers[2].MaxLOD = D3D12_FLOAT32_MAX;
		samplers[2].MipLODBias = 0.0f;
		samplers[2].RegisterSpace = 0;
		samplers[2].ShaderRegister = 2;
		samplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	// Linear wrap
	{
		samplers[3].AddressU = samplers[3].AddressV = samplers[3].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[3].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplers[3].MinLOD = 0.0f;
		samplers[3].MaxLOD = D3D12_FLOAT32_MAX;
		samplers[3].MipLODBias = 0.0f;
		samplers[3].RegisterSpace = 0;
		samplers[3].ShaderRegister = 3;
		samplers[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	// Linear aniso
	{
		samplers[4].AddressU = samplers[4].AddressV = samplers[4].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[4].Filter = D3D12_FILTER_ANISOTROPIC;
		samplers[4].MaxAnisotropy = 8;
		samplers[4].MinLOD = 0.0f;
		samplers[4].MaxLOD = D3D12_FLOAT32_MAX;
		samplers[4].MipLODBias = 0.0f;
		samplers[4].RegisterSpace = 0;
		samplers[4].ShaderRegister = 4;
		samplers[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	D3D12_ROOT_PARAMETER tables[3 * c_numShaderSpaces];
	D3D12_DESCRIPTOR_RANGE ranges[3 * c_numShaderSpaces];

	graphicsDesc.pParameters = tables;
	graphicsDesc.NumParameters = KT_ARRAY_COUNT(tables);

	computeDesc.pParameters = tables;
	computeDesc.NumParameters = KT_ARRAY_COUNT(tables);

	// One dumb global root sig for now.
	// CBV Table (b0 - b16) space0
	// SRV Table (t0 - t16) space0
	// UAV Table (u0 - u16) space0
	// Repeat to space n.

	for (uint32_t i = 0; i < c_numShaderSpaces; ++i)
	{
		ranges[i * 3].BaseShaderRegister = 0;
		ranges[i * 3].NumDescriptors = c_cbvTableSize;
		ranges[i * 3].OffsetInDescriptorsFromTableStart = 0;
		ranges[i * 3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		ranges[i * 3].RegisterSpace = i;

		ranges[i * 3 + 1].BaseShaderRegister = 0;
		ranges[i * 3 + 1].NumDescriptors = c_srvTableSize;
		ranges[i * 3 + 1].OffsetInDescriptorsFromTableStart = 0;
		ranges[i * 3 + 1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[i * 3 + 1].RegisterSpace = i;

		ranges[i * 3 + 2].BaseShaderRegister = 0;
		ranges[i * 3 + 2].NumDescriptors = c_uavTableSize;
		ranges[i * 3 + 2].OffsetInDescriptorsFromTableStart = 0;
		ranges[i * 3 + 2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		ranges[i * 3 + 2].RegisterSpace = i;

		tables[i * 3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		tables[i * 3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		tables[i * 3].DescriptorTable.pDescriptorRanges = &ranges[i * 3];
		tables[i * 3].DescriptorTable.NumDescriptorRanges = 1;

		tables[i * 3 + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		tables[i * 3 + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		tables[i * 3 + 1].DescriptorTable.pDescriptorRanges = &ranges[i * 3 + 1];
		tables[i * 3 + 1].DescriptorTable.NumDescriptorRanges = 1;

		tables[i * 3 + 2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		tables[i * 3 + 2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		tables[i * 3 + 2].DescriptorTable.pDescriptorRanges = &ranges[i * 3 + 2];
		tables[i * 3 + 2].DescriptorTable.NumDescriptorRanges = 1;
	}

	{
		ID3DBlob* rootBlob;
		ID3DBlob* errBlob;
		KT_SCOPE_EXIT(SafeReleaseDX(rootBlob));
		KT_SCOPE_EXIT(SafeReleaseDX(errBlob));

		D3D_CHECK(D3D12SerializeRootSignature(&graphicsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errBlob));

		if (errBlob)
		{
			KT_LOG_ERROR("Failed to create root signature: %s", errBlob->GetBufferPointer());
		}

		D3D_CHECK(_dev->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&o_gfx)));
	}

	{
		ID3DBlob* rootBlob;
		ID3DBlob* errBlob;
		KT_SCOPE_EXIT(SafeReleaseDX(rootBlob));
		KT_SCOPE_EXIT(SafeReleaseDX(errBlob));

		D3D_CHECK(D3D12SerializeRootSignature(&computeDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errBlob));

		if (errBlob)
		{
			KT_LOG_ERROR("Failed to create root signature: %s", errBlob->GetBufferPointer());
		}

		D3D_CHECK(_dev->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&o_compute)));
	}
}

void Device_D3D12::InitDescriptorHeaps()
{
	// Shared heaps resources
	{
		m_rtvHeap.Init(m_d3dDev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, false, "Main RTV Heap");
		m_dsvHeap.Init(m_d3dDev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256, false, "Main DSV Heap");

		m_stagingHeap.Init(m_d3dDev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2048, false, "CBV/SRV/UAV Staging Heap");
	}

	uint32_t constexpr c_totalCbvSrvUavDescriptors = 4096 * 4;

	m_cbvsrvuavHeap.Init(m_d3dDev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, c_totalCbvSrvUavDescriptors, true, "CBV/SRV/UAV GPU Heap");

	m_nullCbv = m_stagingHeap.AllocOne();
	m_nullSrv = m_stagingHeap.AllocOne();
	m_nullUav = m_stagingHeap.AllocOne();

	// TODO: Anything better than this?
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	m_d3dDev->CreateConstantBufferView(nullptr, m_nullCbv);
	m_d3dDev->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_nullUav);
	m_d3dDev->CreateShaderResourceView(nullptr, &srvDesc, m_nullSrv);

	// Create null tables
	uint32_t const totalNullDescriptors = gpu::c_uavTableSize + gpu::c_srvTableSize + gpu::c_cbvTableSize;
	D3D12_CPU_DESCRIPTOR_HANDLE nullTableBeginCpu = m_cbvsrvuavHeap.HandleBeginCPU();
	D3D12_GPU_DESCRIPTOR_HANDLE nullTableBeginGpu = m_cbvsrvuavHeap.HandleBeginGPU();

	m_nullCbvTable = nullTableBeginGpu;
	for (uint32_t i = 0; i < gpu::c_cbvTableSize; ++i)
	{
		m_d3dDev->CopyDescriptorsSimple(1, nullTableBeginCpu, m_nullCbv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		nullTableBeginCpu.ptr += m_cbvsrvuavHeap.m_descriptorIncrementSize;
		nullTableBeginGpu.ptr += m_cbvsrvuavHeap.m_descriptorIncrementSize;
	}

	m_nullSrvTable = nullTableBeginGpu;
	for (uint32_t i = 0; i < gpu::c_srvTableSize; ++i)
	{
		m_d3dDev->CopyDescriptorsSimple(1, nullTableBeginCpu, m_nullSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		nullTableBeginCpu.ptr += m_cbvsrvuavHeap.m_descriptorIncrementSize;
		nullTableBeginGpu.ptr += m_cbvsrvuavHeap.m_descriptorIncrementSize;
	}

	m_nullUavTable = nullTableBeginGpu;
	for (uint32_t i = 0; i < gpu::c_uavTableSize; ++i)
	{
		m_d3dDev->CopyDescriptorsSimple(1, nullTableBeginCpu, m_nullUav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		nullTableBeginCpu.ptr += m_cbvsrvuavHeap.m_descriptorIncrementSize;
		nullTableBeginGpu.ptr += m_cbvsrvuavHeap.m_descriptorIncrementSize;
	}

	m_descriptorcbvsrvuavRingBuffer.Init(&m_cbvsrvuavHeap, totalNullDescriptors, c_totalCbvSrvUavDescriptors - totalNullDescriptors);
}

static void InitMipPsos(Device_D3D12* _dev)
{
	gpu::ShaderBytecode const standardLinear{ g_GenMipsLinear, sizeof(g_GenMipsLinear) };
	gpu::ShaderBytecode const standardSrgb{ g_GenMipsSRGB, sizeof(g_GenMipsSRGB) };
	gpu::ShaderBytecode const cubeLinear{ g_GenMipsCubeLinear, sizeof(g_GenMipsCubeLinear) };
	gpu::ShaderBytecode const cubeSrgb{ g_GenMipsCubeSRGB, sizeof(g_GenMipsCubeSRGB) };
	gpu::ShaderBytecode const arrayLinear{ g_GenMipsArrayLinear, sizeof(g_GenMipsArrayLinear) };
	gpu::ShaderBytecode const arraySrgb{ g_GenMipsArraySRGB, sizeof(g_GenMipsArraySRGB) };

#define MAKE_PSO(_name, _bytecode, _psoOut) \
	KT_MACRO_BLOCK_BEGIN \
	gpu::ShaderRef shader = gpu::CreateShader(gpu::ShaderType::Compute, _bytecode, _name); \
	_psoOut = gpu::CreateComputePSO(shader, _name); \
	KT_MACRO_BLOCK_END

	MAKE_PSO("GenMips_Linear", standardLinear, _dev->m_mipPsos.m_standard[0]);
	MAKE_PSO("GenMips_SRGB", standardSrgb, _dev->m_mipPsos.m_standard[1]);
	MAKE_PSO("GenMips_Array_Linear", arrayLinear, _dev->m_mipPsos.m_array[0]);
	MAKE_PSO("GenMips_Array_SRGB", arraySrgb, _dev->m_mipPsos.m_array[1]);
	MAKE_PSO("GenMips_Cube_Linear", cubeLinear, _dev->m_mipPsos.m_cube[0]);
	MAKE_PSO("GenMips_Cube_SRGB", cubeSrgb, _dev->m_mipPsos.m_cube[1]);
#undef MAKE_PSO
}

void Device_D3D12::Init(void* _nativeWindowHandle, bool _useDebugLayer)
{
	m_resourceHandles.Init(kt::GetDefaultAllocator(), 1024 * 8);
	m_shaderHandles.Init(kt::GetDefaultAllocator(), 1024);
	m_psoHandles.Init(kt::GetDefaultAllocator(), 1024);

	m_withDebugLayer = _useDebugLayer;

	if (_useDebugLayer)
	{
		ID3D12Debug* d3dDebug = nullptr;
		D3D_CHECK(::D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&d3dDebug));
		KT_ASSERT(d3dDebug);
		d3dDebug->EnableDebugLayer();

		ID3D12Debug1 *debug1 = nullptr;

		// TODO: Make this optional
		D3D_CHECK(d3dDebug->QueryInterface(IID_PPV_ARGS(&debug1)));
		if (debug1)
		{
			debug1->SetEnableGPUBasedValidation(true);
			debug1->SetEnableSynchronizedCommandQueueValidation(true);
		}

		SafeReleaseDX(debug1);
		SafeReleaseDX(d3dDebug);
	}


	UINT createFlags = 0;

	if (_useDebugLayer)
	{
		createFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	IDXGIFactory4* dxgiFactory = nullptr;
	D3D_CHECK(CreateDXGIFactory2(createFlags, IID_PPV_ARGS(&dxgiFactory)));
	KT_SCOPE_EXIT(SafeReleaseDX(dxgiFactory));

	IDXGIAdapter4* bestAdaptor = GetBestAdaptor(dxgiFactory);
	KT_SCOPE_EXIT(SafeReleaseDX(bestAdaptor));
	if (!bestAdaptor)
	{
		KT_LOG_ERROR("Failed to find appropriate IDXGIAdapator! Can't init d3d12.");
		return;
	}


	DXGI_ADAPTER_DESC3 desc;
	bestAdaptor->GetDesc3(&desc);

	char nameUtf8[sizeof(desc.Description)];
	::WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameUtf8, sizeof(nameUtf8), nullptr, nullptr);

	m_deviceName = nameUtf8;

	KT_LOG_INFO("Using graphics adaptor: %s, Vendor ID: %u, Device ID: %u", nameUtf8, desc.VendorId, desc.DeviceId);

	HRESULT const hr = ::D3D12CreateDevice(bestAdaptor, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3dDev));

	if (!SUCCEEDED(hr))
	{
		KT_LOG_ERROR("D3D12CreateDevice failed! (HRESULT: %#x)", hr);
		return;
	}

	if (_useDebugLayer)
	{
		ID3D12InfoQueue* infoQueue;
		if (SUCCEEDED(m_d3dDev->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		{
			D3D_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
			D3D_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
			D3D_CHECK(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));

			D3D12_MESSAGE_ID filteredMessages[] = 
			{ 
				// Disabled due to debug layer issue after latest NVidia drivers
				// CPU descriptors are no longer 'virtualized' for quick conversion into actual pointers.
				// See: https://www.gamedev.net/forums/topic/701611-copydescriptorssimple-copying-between-heaps/?do=findComment&comment=5403086
				D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES 
			};

			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.pIDList = filteredMessages;
			filter.DenyList.NumIDs = KT_ARRAY_COUNT(filteredMessages);
			infoQueue->AddRetrievalFilterEntries(&filter);
			infoQueue->AddStorageFilterEntries(&filter);

			infoQueue->Release();
		}
	}

	// Todo: capabilities.
	D3D12_FEATURE_DATA_D3D12_OPTIONS opts;
	D3D12_FEATURE_DATA_D3D12_OPTIONS1 opts1;
	D3D_CHECK(m_d3dDev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts)));
	D3D_CHECK(m_d3dDev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &opts1, sizeof(opts1)));

	m_commandQueueManager.Init(m_d3dDev);


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
		swapChainDesc.BufferCount = gpu::c_maxBufferedFrames;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		IDXGISwapChain1* swapChain;
		D3D_CHECK(dxgiFactory->CreateSwapChainForHwnd(m_commandQueueManager.GraphicsQueue().D3DCommandQueue(), HWND(_nativeWindowHandle), &swapChainDesc, nullptr, nullptr, &swapChain));
		D3D_CHECK(swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain)));
		swapChain->Release();
	}

	// Frame data
	m_uploadPagePool.Init(m_d3dDev);

	InitDescriptorHeaps();

	for (uint32_t i = 0; i < c_maxBufferedFrames; ++i)
	{
		// Create back buffer
		AllocatedResource_D3D12* backbufferData;
		m_backBuffers[i].AcquireNoRef(gpu::TextureHandle{ gpu::ResourceHandle{ m_resourceHandles.Alloc(backbufferData) } });
		ID3D12Resource* backbufferRes;
		D3D_CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backbufferRes)));
		backbufferData->InitFromBackbuffer(backbufferRes, i, Format::R8G8B8A8_UNorm, m_swapChainHeight, m_swapChainWidth);

		m_framesResources[i].m_uploadAllocator.Init(&m_uploadPagePool);
	}

	CreateRootSigs(m_d3dDev, m_graphicsRootSig, m_computeRootSig);

	gpu::TextureDesc const depthDesc = gpu::TextureDesc::Desc2D(m_swapChainWidth, m_swapChainHeight, TextureUsageFlags::DepthStencil, Format::D32_Float);
	m_backbufferDepth.AcquireNoRef(gpu::CreateTexture(depthDesc, nullptr, "Backbuffer Depth"));

	m_cpuFrameIdx = m_swapChain->GetCurrentBackBufferIndex();

	InitMipPsos(g_device);
}

Device_D3D12::Device_D3D12()
{

}

Device_D3D12::~Device_D3D12()
{
	m_mipPsos = MipPSOs{};
	// Sync gpu.
	m_commandQueueManager.FlushAllBlockingCPU();

	// Clear any handles we are holding.
	for (gpu::TextureRef& texRef : m_backBuffers)
	{
		texRef = gpu::TextureRef{};
	}

	m_backbufferDepth = gpu::TextureRef{};

	m_psoCache.Clear();

	KT_ASSERT(m_psoHandles.NumAllocated() == 0);
	KT_ASSERT(m_resourceHandles.NumAllocated() == 0);
	KT_ASSERT(m_shaderHandles.NumAllocated() == 0);

	// TODO: state check/clear handles if not unallocated.

	m_commandQueueManager.Shutdown();

	m_rtvHeap.Shutdown();
	m_dsvHeap.Shutdown();
	m_stagingHeap.Shutdown();

	// Process all deferred deletions, need to clear handles/flush pso etc first so any pending deletions are pushed.
	for (uint32_t i = 0; i < c_maxBufferedFrames; ++i)
	{
		FrameResources& frame = m_framesResources[i];
		frame.ClearOnBeginFrame();
		frame.m_uploadAllocator.Shutdown();
	}

	m_uploadPagePool.Shutdown();
	m_cbvsrvuavHeap.Shutdown();

	// Release final d3d objects.
	SafeReleaseDX(m_d3dDev);
	SafeReleaseDX(m_swapChain);

	SafeReleaseDX(m_graphicsRootSig);
	SafeReleaseDX(m_computeRootSig);
}

gpu::BufferHandle CreateBuffer(gpu::BufferDesc const& _desc, void const* _initialData, char const* _debugName)
{
	AllocatedResource_D3D12* res;
	gpu::ResourceHandle const handle = gpu::ResourceHandle{g_device->m_resourceHandles.Alloc(res)};
	if (!handle.IsValid())
	{
		return gpu::BufferHandle{};
	}

	if (!res->InitAsBuffer(_desc, _initialData, _debugName))
	{
		g_device->m_resourceHandles.Free(handle);
		return gpu::BufferHandle{};
	}

	return gpu::BufferHandle{ handle };
}

gpu::TextureHandle CreateTexture(gpu::TextureDesc const& _desc, void const* _initialData, char const* _debugName)
{
	AllocatedResource_D3D12* res;
	gpu::ResourceHandle const handle = gpu::ResourceHandle{ g_device->m_resourceHandles.Alloc(res) };
	if (!handle.IsValid())
	{
		return gpu::TextureHandle{};
	}

	if (!res->InitAsTexture(_desc, _initialData, _debugName))
	{
		g_device->m_resourceHandles.Free(handle);
		return gpu::TextureHandle{};
	}

	return gpu::TextureHandle{ handle };
}

gpu::ShaderHandle CreateShader(ShaderType _type, gpu::ShaderBytecode const& _byteCode, char const* _debugName)
{
	AllocatedShader_D3D12* shader;
	kt::VersionedHandle handle = g_device->m_shaderHandles.Alloc(shader);
	if (!handle.IsValid())
	{
		return gpu::ShaderHandle{};
	}

	shader->Init(_type, _byteCode, _debugName);
	return gpu::ShaderHandle{ handle };
}

gpu::PSOHandle CreateGraphicsPSO(gpu::GraphicsPSODesc const& _desc, char const* _name)
{
	// Make sure VS and PS are valid.
	AllocatedShader_D3D12* psShader = g_device->m_shaderHandles.Lookup(_desc.m_ps);
	if (!psShader || psShader->m_shaderType != ShaderType::Pixel)
	{
		KT_ASSERT(!"Invalid pixel shader handle passed to CreateGraphicsPSO.");
		return gpu::PSOHandle{};
	}

	AllocatedShader_D3D12* vsShader = g_device->m_shaderHandles.Lookup(_desc.m_vs);
	if (!vsShader || vsShader->m_shaderType != ShaderType::Vertex)
	{
		KT_ASSERT(!"Invalid vertex shader handle passed to CreateGraphicsPSO.");
		return gpu::PSOHandle{};
	}

	uint64_t const hash = kt::XXHash_64(&_desc, sizeof(gpu::GraphicsPSODesc));
	Device_D3D12::PSOCache::Iterator it = g_device->m_psoCache.Find(hash);

	if (it != g_device->m_psoCache.End())
	{
		gpu::PSOHandle const handle = it->m_val;
		AllocatedPSO_D3D12* allocatedPso = g_device->m_psoHandles.Lookup(handle);
		KT_ASSERT(allocatedPso);
		KT_ASSERT(memcmp(&_desc, &allocatedPso->m_psoDesc, sizeof(gpu::GraphicsPSODesc)) == 0);
		allocatedPso->AddRef();
		return handle;
	}

	AllocatedPSO_D3D12* psoData;
	gpu::PSOHandle const psoHandle = gpu::PSOHandle(g_device->m_psoHandles.Alloc(psoData));
	psoData->InitAsGraphics(_desc, vsShader->m_byteCode, psShader->m_byteCode, _name);
	g_device->m_psoCache.Insert(hash, gpu::PSORef{ psoHandle });

	psShader->m_linkedPsos.PushBack(psoHandle);
	vsShader->m_linkedPsos.PushBack(psoHandle);

	return psoHandle;
}

gpu::PSOHandle CreateComputePSO(gpu::ShaderHandle _shader, char const* _name)
{
	AllocatedShader_D3D12* allocatedShader = g_device->m_shaderHandles.Lookup(_shader);
	KT_ASSERT(allocatedShader);
	KT_ASSERT(allocatedShader->m_shaderType == gpu::ShaderType::Compute);
	
	if (allocatedShader->m_linkedPsos.Size() == 0)
	{
		AllocatedPSO_D3D12* psoData;
		gpu::PSOHandle const newPsoHandle = gpu::PSOHandle{ g_device->m_psoHandles.Alloc(psoData) };
		KT_ASSERT(newPsoHandle.IsValid());
		psoData->InitAsCompute(_shader, allocatedShader->m_byteCode, _name);
		// TODO: should shader hold onto a ref?
		allocatedShader->m_linkedPsos.PushBack(newPsoHandle);
		return newPsoHandle;
	}
	else
	{
		KT_ASSERT(allocatedShader->m_linkedPsos.Size() == 1);
		gpu::AddRef(allocatedShader->m_linkedPsos[0]);
		return allocatedShader->m_linkedPsos[0];
	}
}

void SetVsyncEnabled(bool _vsync)
{
	g_device->m_vsync = _vsync;
}

void ReloadShader(ShaderHandle _handle, ShaderBytecode const& _newBytecode)
{
	AllocatedShader_D3D12* shader = g_device->m_shaderHandles.Lookup(_handle);
	if (!shader)
	{
		KT_LOG_ERROR("Attempt to reload shader with invalid handle!");
		return;
	}

	shader->FreeBytecode();
	shader->CopyBytecode(_newBytecode);

	uint32_t numPsos = 0;
	KT_UNUSED(numPsos);

	if (shader->m_shaderType == ShaderType::Compute)
	{
		if (shader->m_linkedPsos.Size() != 0)
		{
			KT_ASSERT(shader->m_linkedPsos.Size() == 1);
			AllocatedPSO_D3D12* pso = g_device->m_psoHandles.Lookup(shader->m_linkedPsos[0]);
			if (!pso)
			{
				shader->m_linkedPsos.Clear();
			}
			else
			{
				if (pso->m_pso)
				{
					g_device->GetFrameResources()->m_deferredDeletions.PushBack(pso->m_pso);
					pso->m_pso = nullptr;
				}
				pso->m_pso = CreateD3DComputePSO(g_device->m_d3dDev, shader->m_byteCode);
			}
		}
	}
	else
	{
		for (kt::Array<gpu::PSOHandle>::Iterator it = shader->m_linkedPsos.Begin();
			 it != shader->m_linkedPsos.End();
			 /* */)
		{
			AllocatedPSO_D3D12* psoData = g_device->m_psoHandles.Lookup(*it);
			if (!psoData)
			{
				it = shader->m_linkedPsos.EraseSwap(it);
				continue;
			}

			ShaderBytecode const& vsBytecode = g_device->m_shaderHandles.Lookup(psoData->m_psoDesc.m_vs)->m_byteCode;
			ShaderBytecode const& psBytecode = g_device->m_shaderHandles.Lookup(psoData->m_psoDesc.m_ps)->m_byteCode;

			if (psoData->m_pso)
			{
				g_device->GetFrameResources()->m_deferredDeletions.PushBack(psoData->m_pso);
				psoData->m_pso = nullptr;
			}

			psoData->m_pso = CreateD3DGraphicsPSO(g_device->m_d3dDev, psoData->m_psoDesc, vsBytecode, psBytecode);
			++numPsos;
			++it;
		}
	}


	KT_LOG_INFO("Shader \"%s\" reloaded, %u PSO(s) re-compiled.", shader->m_debugName.Data(), numPsos);
}



gpu::TextureHandle CurrentBackbuffer()
{
	return g_device->m_backBuffers[g_device->m_cpuFrameIdx].Handle();
}

gpu::TextureHandle BackbufferDepth()
{
	return g_device->m_backbufferDepth;
}

template <typename HandleT, typename DataT>
static void AddRefImpl(HandleT _handle, kt::VersionedHandlePool<DataT>& _pool)
{
	KT_ASSERT(_pool.IsValid(_handle));
	_pool.Lookup(_handle)->AddRef();
}

template <typename HandleT, typename DataT>
static void ReleaseRefImpl(HandleT _handle, kt::VersionedHandlePool<DataT>& _pool)
{
	KT_ASSERT(_pool.IsValid(_handle));
	DataT* data = _pool.Lookup(_handle);
	if (data->ReleaseRef() == 0)
	{
		data->Destroy();
		_pool.Free(_handle);
	}
}

void AddRef(gpu::ResourceHandle _handle)
{
	AddRefImpl(_handle, g_device->m_resourceHandles);
}

void AddRef(gpu::ShaderHandle _handle)
{
	AddRefImpl(_handle, g_device->m_shaderHandles);
}

void AddRef(gpu::PSOHandle _handle)
{
	AddRefImpl(_handle, g_device->m_psoHandles);
}

void Release(gpu::ResourceHandle _handle)
{
	ReleaseRefImpl(_handle, g_device->m_resourceHandles);
}

void Release(gpu::ShaderHandle _handle)
{
	ReleaseRefImpl(_handle, g_device->m_shaderHandles);
}

void Release(gpu::PSOHandle _handle)
{
	ReleaseRefImpl(_handle, g_device->m_psoHandles);
}

bool Init(void* _nwh)
{
	KT_ASSERT(!g_device);
	// TODO: Toggle debug layer
	g_device = new Device_D3D12();
	// TODO
	g_device->Init(_nwh, true);
	return true;
}

void Shutdown()
{
	KT_ASSERT(g_device);
	delete g_device;
}

void BeginFrame()
{
	g_device->BeginFrame();
}

void EndFrame()
{
	g_device->EndFrame();
}

void Device_D3D12::BeginFrame()
{
	m_commandQueueManager.WaitForFenceBlockingCPU(m_frameFences[m_cpuFrameIdx]);

	m_framesResources[m_cpuFrameIdx].ClearOnBeginFrame();
	m_descriptorcbvsrvuavRingBuffer.OnBeginFrame(m_cpuFrameIdx);

	m_mainThreadCtx = gpu::cmd::Begin(gpu::cmd::ContextType::Graphics);
	gpu::cmd::ResourceBarrier(m_mainThreadCtx, m_backBuffers[m_cpuFrameIdx], gpu::ResourceState::RenderTarget);
	gpu::cmd::FlushBarriers(m_mainThreadCtx);
}

void Device_D3D12::EndFrame()
{
	gpu::cmd::ResourceBarrier(m_mainThreadCtx, m_backBuffers[m_cpuFrameIdx], gpu::ResourceState::Present);
	gpu::cmd::End(m_mainThreadCtx);
	m_mainThreadCtx = nullptr;

	m_descriptorcbvsrvuavRingBuffer.OnEndOfFrame(m_cpuFrameIdx);

	D3D_CHECK(m_swapChain->Present(m_vsync ? 1 : 0, 0));
	m_frameFences[m_cpuFrameIdx] = m_commandQueueManager.GraphicsQueue().InsertAndIncrementFence();
	m_cpuFrameIdx = m_swapChain->GetCurrentBackBufferIndex();
	++m_frameCounter;
}

gpu::cmd::Context* GetMainThreadCommandCtx()
{
	KT_ASSERT(g_device->m_mainThreadCtx);
	return g_device->m_mainThreadCtx;
}

void GetSwapchainDimensions(uint32_t& o_width, uint32_t& o_height)
{
	o_width = g_device->m_swapChainWidth;
	o_height = g_device->m_swapChainHeight;
}

gpu::Format BackbufferFormat()
{
	return g_device->m_resourceHandles.Lookup(g_device->m_backBuffers[0])->m_textureDesc.m_format;
}

gpu::Format BackbufferDepthFormat()
{
	return g_device->m_resourceHandles.Lookup(g_device->m_backbufferDepth)->m_textureDesc.m_format;
}

void Device_D3D12::FrameResources::ClearOnBeginFrame()
{
	for (ID3D12DeviceChild* obj: m_deferredDeletions)
	{
		KT_ASSERT(obj);
		obj->Release();
	}

	m_deferredDeletions.Clear();

	m_uploadAllocator.ClearOnBeginFrame();
}

// Why is this in platform specific code?
// Becuase I don't have any other need for subresource barriers, and don't want to add them just now for extra complexity.
// It seems that having the whole resource in D3D12_RESOURCE_STATE_UNORDERED_ACCESS works on some cards (and this is what MiniEngine does).
// However this isn't technically correct, and throws GPU-Based validation errors.
void GenerateMips(gpu::cmd::Context* _ctx, gpu::ResourceHandle _handle)
{
	AllocatedResource_D3D12* res = g_device->m_resourceHandles.Lookup(_handle);
	KT_ASSERT(res);

	gpu::ResourceType type = res->m_type;
	KT_ASSERT(gpu::IsTexture(type));
	gpu::TextureDesc const& desc = res->m_textureDesc;

	bool const srgb = gpu::IsSRGBFormat(desc.m_format);

	gpu::PSORef genMipsPso;
	uint32_t arraySlices = desc.m_arraySlices;

	if (type == gpu::ResourceType::TextureCube)
	{
		KT_ASSERT(desc.m_arraySlices == 1 && "Cube array mip gen not supported.");
		genMipsPso = g_device->m_mipPsos.m_cube[srgb];
		arraySlices = 6;
	}
	else if (desc.m_arraySlices > 1)
	{
		genMipsPso = g_device->m_mipPsos.m_array[srgb];
	}
	else
	{
		genMipsPso = g_device->m_mipPsos.m_standard[srgb];
	}


	struct MipCbuf
	{
		uint32_t srcMip;
		uint32_t numOutputMips;
		kt::Vec2 rcpTexelSize;
	} cbuf;

	uint32_t const totalMips = desc.m_mipLevels;

	uint32_t srcMip = 0;

	uint32_t destMipStart = 1;

	gpu::cmd::SetPSO(_ctx, genMipsPso);

	kt::Array<D3D12_RESOURCE_BARRIER> barrierArray;
	barrierArray.Reserve(arraySlices * totalMips);

	auto transitionSubresourceArrayFn = [arraySlices, res, totalMips, &barrierArray](D3D12_RESOURCE_STATES _before, D3D12_RESOURCE_STATES _after, uint32_t _mip)
	{
		D3D12_RESOURCE_BARRIER* barriers = barrierArray.PushBack_Raw(arraySlices);
		for (uint32_t i = 0; i < arraySlices; ++i)
		{
			barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barriers[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[i].Transition.pResource = res->m_res;
			barriers[i].Transition.StateBefore = _before;
			barriers[i].Transition.StateAfter = _after;
			barriers[i].Transition.Subresource = D3DSubresourceIndex(_mip, i, totalMips);
		}
	};

	// Need to handle subresource barriers, currently not exposed outside of here since I have no other good uses for them at the moment.
	if (res->m_resState == gpu::ResourceState::UnorderedAccess)
	{
		transitionSubresourceArrayFn(D3DTranslateResourceState(res->m_resState), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);
	}
	else if(res->m_resState == ResourceState::ShaderResource)
	{
		for (uint32_t i = 1; i < totalMips; ++i)
		{
			transitionSubresourceArrayFn(D3DTranslateResourceState(res->m_resState), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, i);
		}
	}
	else
	{
		transitionSubresourceArrayFn(D3DTranslateResourceState(res->m_resState), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0);

		for (uint32_t i = 1; i < totalMips; ++i)
		{
			transitionSubresourceArrayFn(D3DTranslateResourceState(res->m_resState), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, i);
		}
	}

	_ctx->m_cmdList->ResourceBarrier(barrierArray.Size(), barrierArray.Data());
	barrierArray.Clear();

	gpu::DescriptorData srvDescriptor;
	srvDescriptor.Set(_handle);
	gpu::cmd::SetComputeSRVTable(_ctx, srvDescriptor, 0);

	while (destMipStart < totalMips)
	{
		uint32_t const c_maxMipsPerPass = 4;

		uint32_t const xDim = kt::Max<uint32_t>(1u, desc.m_width >> destMipStart); 
		uint32_t const yDim = kt::Max<uint32_t>(1u, desc.m_height >> destMipStart);
		cbuf.rcpTexelSize.x = 1.0f / float(xDim);
		cbuf.rcpTexelSize.y = 1.0f / float(yDim);
		cbuf.srcMip = srcMip;
		cbuf.numOutputMips = kt::Min(c_maxMipsPerPass, totalMips - destMipStart);

		gpu::DescriptorData cbufDescriptor;
		gpu::DescriptorData uavDescriptor[c_maxMipsPerPass];

		cbufDescriptor.Set(&cbuf, sizeof(cbuf));

		for (uint32_t i = 0; i < cbuf.numOutputMips; ++i)
		{
			uavDescriptor[i].Set(_handle, destMipStart + i);
		}

		gpu::cmd::SetComputeUAVTable(_ctx, uavDescriptor, 0);
		gpu::cmd::SetComputeCBVTable(_ctx, cbufDescriptor, 0);

		uint32_t constexpr c_genMipDim = 8;
		gpu::cmd::Dispatch(_ctx, uint32_t(kt::AlignUp(xDim, c_genMipDim)) / c_genMipDim, uint32_t(kt::AlignUp(xDim, c_genMipDim)) / c_genMipDim, arraySlices);

		for (uint32_t i = destMipStart; i < destMipStart + cbuf.numOutputMips; ++i)
		{
			transitionSubresourceArrayFn(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, i);
		}

		destMipStart += cbuf.numOutputMips;
		cbuf.srcMip += cbuf.numOutputMips;

		D3D12_RESOURCE_BARRIER& uavBarrier = barrierArray.PushBack();
		uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBarrier.UAV.pResource = res->m_res;
		_ctx->m_cmdList->ResourceBarrier(barrierArray.Size(), barrierArray.Data());
		barrierArray.Clear();
	}

	// At this point everything has been transitioned back to shader resources, so no invariants are broken.
	res->m_resState = ResourceState::ShaderResource;
}

void EnumResourceHandles(kt::StaticFunction<void(gpu::ResourceHandle), 32> const& _ftor)
{
	kt::VersionedHandlePool<AllocatedResource_D3D12>& handlePool = g_device->m_resourceHandles;

	for (uint32_t i = handlePool.FirstAllocatedIndex(); 
		 handlePool.IsIndexInUse(i); 
		 i = handlePool.NextAllocatedIndex(i))
	{
		_ftor(gpu::ResourceHandle{ handlePool.HandleForIndex(i) });
	}
}

bool GetResourceInfo(gpu::ResourceHandle _handle, gpu::ResourceType& _type, gpu::BufferDesc* o_bufferDesc, gpu::TextureDesc* o_textureDesc, char const** o_name)
{
	if (AllocatedResource_D3D12* res = g_device->m_resourceHandles.Lookup(_handle))
	{
		if (o_name)
		{
			*o_name = res->m_debugName.Data();
		}
		
		_type = res->m_type;
		if (res->IsTexture())
		{
			if (o_textureDesc)
			{
				*o_textureDesc = res->m_textureDesc;
			}
		}
		else if (o_bufferDesc)
		{
			*o_bufferDesc = res->m_bufferDesc;
		}
		return true;
	}

	return false;
}

bool GetShaderInfo(ShaderHandle _handle, ShaderType& o_type, char const*& o_name)
{
	if (AllocatedShader_D3D12* shader = g_device->m_shaderHandles.Lookup(_handle))
	{
		o_type = shader->m_shaderType;
		o_name = shader->m_debugName.Data();
		return true;
	}

	return false;
}



}