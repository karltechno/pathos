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

#include <string.h>

static D3D12_HEAP_PROPERTIES const c_defaultHeapProperties{ D3D12_HEAP_TYPE_DEFAULT , D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
static D3D12_HEAP_PROPERTIES const c_uploadHeapProperties{ D3D12_HEAP_TYPE_UPLOAD , D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

namespace gpu
{

Device_D3D12* g_device = nullptr;

static uint32_t GetBufferAlign(gpu::BufferDesc const& _desc)
{
	if (!!(_desc.m_flags & gpu::BufferFlags::Constant))
	{
		return D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	}
	else
	{
		return 16; // Anything better ?
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

static void CopyInitialTextureData(AllocatedTexture_D3D12& _tex, void const* _data, D3D12_RESOURCE_STATES _beforeState, D3D12_RESOURCE_STATES _afterState)
{

	// TODO: This doesn't handle subresources currently.
	// TODO: Always assume initialdata is correct for desc?

	UINT64 totalBytes;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	UINT numRows;
	UINT64 rowSizes;
	D3D12_RESOURCE_DESC const d3dDesc = _tex.m_res->GetDesc();
	g_device->m_d3dDev->GetCopyableFootprints(&d3dDesc, 0, 1, 0, &footprint, &numRows, &rowSizes, &totalBytes);
	KT_ASSERT(totalBytes);
	ScratchAlloc_D3D12 uploadScratch = g_device->GetFrameResources()->m_uploadAllocator.Alloc(uint32_t(totalBytes), 16);
	
	uint8_t* srcPtr = (uint8_t*)_data;
	uint8_t* destPtr = (uint8_t*)uploadScratch.m_cpuData + footprint.Offset;

	uint32_t const srcPitch = gpu::GetFormatSize(_tex.m_desc.m_format) * _tex.m_desc.m_width;
	
	for (uint32_t rowIdx = 0; rowIdx < numRows; ++rowIdx)
	{
		memcpy(destPtr, srcPtr, srcPitch);
		srcPtr += srcPitch;
		destPtr += footprint.Footprint.RowPitch;
	}

	D3D12_TEXTURE_COPY_LOCATION destLoc;
	destLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destLoc.pResource = _tex.m_res;
	destLoc.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION srcLoc;
	srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLoc.pResource = uploadScratch.m_res;
	srcLoc.PlacedFootprint = footprint;
	srcLoc.PlacedFootprint.Offset += uploadScratch.m_offset; // TODO: Is this undefined with alignment? Do we need to pass it into GetCopyableFootprints

	ID3D12CommandAllocator* allocator = g_device->m_commandQueueManager.GraphicsQueue().AcquireAllocator();
	// TODO: Pool lists
	ID3D12CommandList* listBase;
	// TODO: Use direct queue for now, need to work out synchronization and use copy queue.
	D3D_CHECK(g_device->m_d3dDev->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&listBase)));
	ID3D12GraphicsCommandList* list = (ID3D12GraphicsCommandList*)listBase;
	list->CopyTextureRegion(&destLoc, 0, 0, 0, &srcLoc, nullptr);

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

bool AllocatedBuffer_D3D12::Init(BufferDesc const& _desc, void const* _initialData, char const* _debugName)
{
	KT_ASSERT(!m_res);

	m_desc = _desc;

	D3D12_RESOURCE_STATES bestInitialState = D3D12_RESOURCE_STATE_COMMON;
	if (!!(_desc.m_flags & (BufferFlags::Constant | BufferFlags::Vertex)))
	{
		bestInitialState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	}
	else if (!!(_desc.m_flags & BufferFlags::Index))
	{
		bestInitialState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	}
	else if (!!(_desc.m_flags & BufferFlags::ShaderResource))
	{
		bestInitialState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
	else if (!!(_desc.m_flags & BufferFlags::UnorderedAccess))
	{
		bestInitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	D3D12_RESOURCE_STATES const creationState = _initialData ? D3D12_RESOURCE_STATE_COPY_DEST : bestInitialState;

	m_state = bestInitialState;
	if (_initialData)
	{
		m_state = D3D12_RESOURCE_STATE_COPY_DEST;
	}

	if (!!(_desc.m_flags & BufferFlags::Constant))
	{
		m_cbv = g_device->m_stagingHeap.AllocOne();
	}

	if (!!(_desc.m_flags & BufferFlags::UnorderedAccess))
	{
		m_uav = g_device->m_stagingHeap.AllocOne();
	}

	if (!(_desc.m_flags & BufferFlags::Transient))
	{
		m_ownsResource = true;
		// Create a committed buffer if not transient. 
		// TODO: Make an actual heap allocator.

		D3D12_RESOURCE_DESC desc{};
		desc.Alignment = 0;
		desc.DepthOrArraySize = 1;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Flags = !!(_desc.m_flags & BufferFlags::UnorderedAccess) ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Height = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Width = _desc.m_sizeInBytes;

		if (!SUCCEEDED(g_device->m_d3dDev->CreateCommittedResource(&c_defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, creationState, nullptr, IID_PPV_ARGS(&m_res))))
		{
			KT_LOG_ERROR("CreateCommittedResource failed to create buffer size %u.", _desc.m_sizeInBytes);
			return false;
		}

		m_gpuAddress = m_res->GetGPUVirtualAddress();
		m_offset = 0;

		UpdateViews();
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
			memcpy(m_mappedCpuData, _initialData, _desc.m_sizeInBytes);
			UpdateViews();
		}
		else
		{
			KT_ASSERT(m_res);
			CopyInitialResourceData(m_res, _initialData, _desc.m_sizeInBytes, creationState, m_state);
		}
	}

	return true;
}

void AllocatedBuffer_D3D12::Destroy()
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

	if (m_uav.ptr)
	{
		g_device->m_stagingHeap.Free(m_uav);
		m_uav.ptr = 0;
	}

	if (m_cbv.ptr)
	{
		g_device->m_stagingHeap.Free(m_cbv);
		m_cbv.ptr = 0;
	}

	m_mappedCpuData = nullptr;
	m_lastFrameTouched = 0xFFFFFFFF;
}


void AllocatedBuffer_D3D12::UpdateViews()
{
	if (!!(m_desc.m_flags & BufferFlags::Constant))
	{
		KT_ASSERT(m_cbv.ptr);
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
		cbvDesc.BufferLocation = m_gpuAddress;
		cbvDesc.SizeInBytes = m_desc.m_sizeInBytes;
		g_device->m_d3dDev->CreateConstantBufferView(&cbvDesc, m_cbv);
	}

	if (!!(m_desc.m_flags & BufferFlags::ShaderResource))
	{
		KT_ASSERT(m_srv.ptr);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = ToDXGIFormat(m_desc.m_format);
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = m_desc.m_strideInBytes / m_desc.m_sizeInBytes;
		srvDesc.Buffer.StructureByteStride = m_desc.m_strideInBytes;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		g_device->m_d3dDev->CreateShaderResourceView(m_res, &srvDesc, m_srv);
	}
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
		kt::GetDefaultAllocator()->FreeSized(m_byteCode.m_data, m_byteCode.m_size);
		m_byteCode = ShaderBytecode{};
	}

	void CopyBytecode(ShaderBytecode const& _byteCode)
	{
		KT_ASSERT(!m_byteCode.m_data);
		m_byteCode.m_data = kt::GetDefaultAllocator()->Alloc(_byteCode.m_size);
		m_byteCode.m_size = _byteCode.m_size;
		memcpy(m_byteCode.m_data, _byteCode.m_data, _byteCode.m_size);
	}

	ShaderBytecode m_byteCode;
	ShaderType m_shaderType;
	
	kt::InplaceArray<GraphicsPSOHandle, 4> m_linkedPsos;
};

void AllocatedGraphicsPSO_D3D12::Init
(
	ID3D12Device* _device, 
	gpu::GraphicsPSODesc const& _desc, 
	gpu::ShaderBytecode const& _vs, 
	gpu::ShaderBytecode const& _ps, 
	gpu::ShaderHandle _vsHandle, 
	gpu::ShaderHandle _pshandle
)
{
	gpu::AddRef(_vsHandle);
	gpu::AddRef(_pshandle);

	m_vs = _vsHandle;
	m_ps = _pshandle;
	m_psoDesc = _desc;

	CreateD3DPSO(_device, _desc, _vs, _ps);

	// TODO: Debug name
	AllocatedObjectBase_D3D12::Init(nullptr);
}

void AllocatedGraphicsPSO_D3D12::CreateD3DPSO(ID3D12Device* _device, gpu::GraphicsPSODesc const& _desc, gpu::ShaderBytecode const& _vs, gpu::ShaderBytecode const& _ps)
{
	KT_ASSERT(!m_pso);

	// Create a new PSO.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3dDesc{};

	d3dDesc.VS = D3D12_SHADER_BYTECODE{ _vs.m_data, _vs.m_size };
	d3dDesc.PS = D3D12_SHADER_BYTECODE{ _ps.m_data, _ps.m_size };

	d3dDesc.pRootSignature = g_device->m_debugRootSig;

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

	d3dDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // TODO

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

	D3D_CHECK(_device->CreateGraphicsPipelineState(&d3dDesc, IID_PPV_ARGS(&m_pso)));
}


bool AllocatedTexture_D3D12::Init(TextureDesc const& _desc, void const* _initialData, char const* _debugName /* = nullptr */)
{
	KT_ASSERT(!m_res);
	m_desc = _desc;

	m_ownsResource = true;

	D3D12_RESOURCE_STATES bestInitialState = D3D12_RESOURCE_STATE_COMMON;

	// TODO: Something better?
	if (!!(_desc.m_usageFlags & TextureUsageFlags::DepthStencil))
	{
		bestInitialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
	else if (!!(_desc.m_usageFlags & TextureUsageFlags::RenderTarget))
	{
		bestInitialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	else if (!!(_desc.m_usageFlags & TextureUsageFlags::ShaderResource))
	{
		bestInitialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	}
	else if (!!(_desc.m_usageFlags & TextureUsageFlags::UnorderedAccess))
	{
		bestInitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}


	D3D12_RESOURCE_STATES const creationState = _initialData ? D3D12_RESOURCE_STATE_COPY_DEST : bestInitialState;
	m_state = creationState;

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
	d3dDesc.DepthOrArraySize = _desc.m_depth;
	d3dDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dDesc.MipLevels = _desc.m_mipLevels;
	d3dDesc.Format = ToDXGIFormat(_desc.m_format);
	d3dDesc.Alignment = 0;

	// TODO: MSAA
	d3dDesc.SampleDesc.Count = 1;
	d3dDesc.SampleDesc.Quality = 0;

	switch (_desc.m_type)
	{
		case TextureType::Texture1D:
		{
			d3dDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
			d3dDesc.Width = _desc.m_width;
		} break;

		case TextureType::Texture2D:
		{
			d3dDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			d3dDesc.Width = _desc.m_width;
			d3dDesc.Height = _desc.m_height;
		} break;

		case TextureType::Texture3D:
		{
			d3dDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
			d3dDesc.Width = _desc.m_width;
			d3dDesc.Height = _desc.m_height;
			d3dDesc.DepthOrArraySize = _desc.m_depth;
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
		depthClear.DepthStencil.Depth = 1.0f; // TODO: Selectable for reverse Z?
	}

	HRESULT const hr = g_device->m_d3dDev->CreateCommittedResource(&c_defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &d3dDesc, creationState, pClearVal, IID_PPV_ARGS(&m_res));
	if (!SUCCEEDED(hr))
	{
		KT_LOG_ERROR("CreateCommittedResource failed (HRESULT: %u)", hr);
		KT_ASSERT(false);
		return false;
	}

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

	if (!!(_desc.m_usageFlags & gpu::TextureUsageFlags::ShaderResource))
	{
		m_srv = g_device->m_stagingHeap.AllocOne();
		g_device->m_d3dDev->CreateShaderResourceView(m_res, nullptr, m_srv);
	}

	AllocatedObjectBase_D3D12::Init(_debugName);
	if (m_res)
	{
		D3D_SET_DEBUG_NAME(m_res, m_debugName.Data());
	}

	if (_initialData)
	{
		CopyInitialTextureData(*this, _initialData, m_state, bestInitialState);
	}

	return true;
}

void AllocatedTexture_D3D12::InitFromBackbuffer(ID3D12Resource* _res, gpu::Format _format, uint32_t _height, uint32_t _width)
{
	m_res = _res;
	m_ownsResource = true;
	
	m_desc = TextureDesc::Desc2D(_width, _height, TextureUsageFlags::RenderTarget, _format);

	m_rtv = g_device->m_rtvHeap.AllocOne();
	m_srv = g_device->m_stagingHeap.AllocOne();

	m_state = D3D12_RESOURCE_STATE_RENDER_TARGET;

	g_device->m_d3dDev->CreateRenderTargetView(m_res, nullptr, m_rtv);
	g_device->m_d3dDev->CreateShaderResourceView(m_res, nullptr, m_srv);

	AllocatedObjectBase_D3D12::Init("Backbuffer");

	if (m_res)
	{
		D3D_SET_DEBUG_NAME(m_res, m_debugName.Data());
	}
}

void AllocatedTexture_D3D12::Destroy()
{
	if (m_res && m_ownsResource)
	{
		g_device->GetFrameResources()->m_deferredDeletions.PushBack(m_res);
	}

	m_res = nullptr;

	if (m_srv.ptr)
	{
		g_device->m_stagingHeap.Free(m_srv);
		m_srv.ptr = 0;
	}

	if (m_rtv.ptr)
	{
		g_device->m_rtvHeap.Free(m_rtv);
		m_dsv.ptr = 0;
	}

	if (m_dsv.ptr)
	{
		g_device->m_dsvHeap.Free(m_dsv);
		m_dsv.ptr = 0;
	}
}

void AllocatedGraphicsPSO_D3D12::Destroy()
{
	if (m_pso)
	{
		g_device->GetFrameResources()->m_deferredDeletions.PushBack(m_pso);
		m_pso = nullptr;
	}

	if (m_ps.IsValid())
	{
		gpu::Release(m_ps);
		m_ps = gpu::ShaderHandle{};
	}

	if (m_vs.IsValid())
	{
		gpu::Release(m_vs);
		m_vs = gpu::ShaderHandle{};
	}
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
	FrameUploadPage_D3D12& page = m_freePages.PushBack(_page);
	page.m_curOffset = 0;
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
			page = &m_pages.Back();
		}

		uintptr_t const alignedAddr = kt::AlignUp(page->m_base + page->m_curOffset, _align);
		uintptr_t const endAddr = alignedAddr + _size;
		if (endAddr > page->m_base + page->m_size)
		{
			++m_numFullPages;
			continue;
		}

		page->m_curOffset = endAddr - page->m_base;
		
		ScratchAlloc_D3D12 alloc;

		alloc.m_offset = alignedAddr - page->m_base;
		alloc.m_addr = page->m_base + alloc.m_offset;
		alloc.m_res = page->m_res;
		alloc.m_cpuData = (uint8_t*)page->m_mappedPtr + alloc.m_offset;
		return alloc;

	} while (false);

	KT_UNREACHABLE;
}

void FrameUploadAllocator_D3D12::Alloc(AllocatedBuffer_D3D12& o_res)
{
	ScratchAlloc_D3D12 scratch = Alloc(o_res.m_desc.m_sizeInBytes, GetBufferAlign(o_res.m_desc));
	o_res.m_mappedCpuData = scratch.m_cpuData;
	o_res.m_res = scratch.m_res;
	o_res.m_offset = scratch.m_offset;
	o_res.m_gpuAddress = scratch.m_addr;

	o_res.m_ownsResource = false;
	o_res.m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
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


static ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device* _dev)
{
	D3D12_ROOT_SIGNATURE_DESC desc = {};

	D3D12_STATIC_SAMPLER_DESC samplers[4] = {};
	desc.pStaticSamplers = samplers;
	desc.NumStaticSamplers = KT_ARRAY_COUNT(samplers);

	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

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
		samplers[1].AddressU = samplers[1].AddressV = samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
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

	D3D12_ROOT_PARAMETER tables[3 * c_numShaderSpaces];
	D3D12_DESCRIPTOR_RANGE ranges[3 * c_numShaderSpaces];

	desc.pParameters = tables;
	desc.NumParameters = KT_ARRAY_COUNT(tables);

	// One dumb global root sig for now.
	// CBV Table (b0 - b16) space0
	// SRV Table (t0 - t16) space0
	// UAV Table (u0 - u16) space0
	// CBV Table (b0 - b16) space1
	// SRV Table (t0 - t16) space1
	// UAV Table (u0 - u16) space1

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


	ID3DBlob* rootBlob;
	ID3DBlob* errBlob;
	KT_SCOPE_EXIT(SafeReleaseDX(rootBlob));
	KT_SCOPE_EXIT(SafeReleaseDX(errBlob));

	D3D_CHECK(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errBlob));

	if (errBlob)
	{
		KT_LOG_ERROR("Failed to create root signature: %s", errBlob->GetBufferPointer());
	}

	ID3D12RootSignature* rsig;

	D3D_CHECK(_dev->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&rsig)));
	return rsig;
}

void Device_D3D12::Init(void* _nativeWindowHandle, bool _useDebugLayer)
{
	m_bufferHandles.Init(kt::GetDefaultAllocator(), 1024 * 8);
	m_shaderHandles.Init(kt::GetDefaultAllocator(), 1024);
	m_textureHandles.Init(kt::GetDefaultAllocator(), 1024 * 4);
	m_psoHandles.Init(kt::GetDefaultAllocator(), 1024);

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
			infoQueue->Release();
		}
	}

	m_commandQueueManager.Init(m_d3dDev);

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
		swapChainDesc.BufferCount = gpu::c_maxBufferedFrames;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

		D3D_CHECK(dxgiFactory->CreateSwapChainForHwnd(m_commandQueueManager.GraphicsQueue().D3DCommandQueue(), HWND(_nativeWindowHandle), &swapChainDesc, nullptr, nullptr, &m_swapChain));
	}

	// Shared heaps resources
	{
		m_rtvHeap.Init(m_d3dDev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, false, "Main RTV Heap");
		m_dsvHeap.Init(m_d3dDev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256, false, "Main DSV Heap");

		m_stagingHeap.Init(m_d3dDev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, false, "CBV/SRV/UAV Staging Heap");
	}

	// Frame data

	m_uploadPagePool.Init(m_d3dDev);

	uint32_t constexpr c_totalCbvSrvUavDescriptors = 4096 * 4;

	m_cbvsrvuavHeap.Init(m_d3dDev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, c_totalCbvSrvUavDescriptors, true, "CBV/SRV/UAV GPU Heap");
	m_descriptorcbvsrvuavRingBuffer.Init(&m_cbvsrvuavHeap, 0, c_totalCbvSrvUavDescriptors);

	for (uint32_t i = 0; i < c_maxBufferedFrames; ++i)
	{
		// Create back buffer
		AllocatedTexture_D3D12* backbufferData;
		m_backBuffers[i].AcquireNoRef(gpu::TextureHandle(m_textureHandles.Alloc(backbufferData)));
		ID3D12Resource* backbufferRes;
		D3D_CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backbufferRes)));
		backbufferData->InitFromBackbuffer(backbufferRes, Format::R8G8B8A8_UNorm, m_swapChainHeight, m_swapChainWidth);

		m_framesResources[i].m_uploadAllocator.Init(&m_uploadPagePool);
	}

	//DebugCreateGraphicsPSO();
	m_debugRootSig = CreateGraphicsRootSignature(m_d3dDev);

	gpu::TextureDesc const depthDesc = gpu::TextureDesc::Desc2D(m_swapChainWidth, m_swapChainHeight, TextureUsageFlags::DepthStencil, Format::D32_Float);
	m_backbufferDepth.AcquireNoRef(gpu::CreateTexture(depthDesc, nullptr, "Backbuffer Depth"));

	m_nullCbv = m_stagingHeap.AllocOne();
	m_d3dDev->CreateConstantBufferView(nullptr, m_nullCbv);
}

Device_D3D12::Device_D3D12()
{

}

Device_D3D12::~Device_D3D12()
{
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
	KT_ASSERT(m_textureHandles.NumAllocated() == 0);
	KT_ASSERT(m_bufferHandles.NumAllocated() == 0);
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

	SafeReleaseDX(m_debugRootSig);
	SafeReleaseDX(m_debugPsoTest);
}

gpu::BufferHandle CreateBuffer(gpu::BufferDesc const& _desc, void const* _initialData, char const* _debugName)
{
	AllocatedBuffer_D3D12* res;
	gpu::BufferHandle const handle = gpu::BufferHandle(g_device->m_bufferHandles.Alloc(res));
	if (!handle.IsValid())
	{
		return gpu::BufferHandle{};
	}

	if (!res->Init(_desc, _initialData, _debugName))
	{
		g_device->m_bufferHandles.Free(handle);
		return gpu::BufferHandle{};
	}

	return gpu::BufferHandle{ handle };
}

gpu::TextureHandle CreateTexture(gpu::TextureDesc const& _desc, void const* _initialData, char const* _debugName)
{
	AllocatedTexture_D3D12* res;
	kt::VersionedHandle const handle = g_device->m_textureHandles.Alloc(res);
	if (!handle.IsValid())
	{
		return gpu::TextureHandle{};
	}

	if (!res->Init(_desc, _initialData, _debugName))
	{
		g_device->m_bufferHandles.Free(handle);
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

gpu::GraphicsPSOHandle CreateGraphicsPSO(gpu::GraphicsPSODesc const& _desc)
{
	// Make sure VS and PS are valid.
	AllocatedShader_D3D12* psShader = g_device->m_shaderHandles.Lookup(_desc.m_ps);
	if (!psShader || psShader->m_shaderType != ShaderType::Pixel)
	{
		KT_ASSERT(!"Invalid pixel shader handle passed to CreateGraphicsPSO.");
		return gpu::GraphicsPSOHandle{};
	}

	AllocatedShader_D3D12* vsShader = g_device->m_shaderHandles.Lookup(_desc.m_vs);
	if (!vsShader || vsShader->m_shaderType != ShaderType::Vertex)
	{
		KT_ASSERT(!"Invalid vertex shader handle passed to CreateGraphicsPSO.");
		return gpu::GraphicsPSOHandle{};
	}

	uint64_t const hash = kt::XXHash_64(&_desc, sizeof(gpu::GraphicsPSODesc));
	Device_D3D12::PSOCache::Iterator it = g_device->m_psoCache.Find(hash);

	if (it != g_device->m_psoCache.End())
	{
		gpu::GraphicsPSOHandle const handle = it->m_val;
		AllocatedGraphicsPSO_D3D12* allocatedPso = g_device->m_psoHandles.Lookup(handle);
		KT_ASSERT(allocatedPso);
		KT_ASSERT(memcmp(&_desc, &allocatedPso->m_psoDesc, sizeof(gpu::GraphicsPSODesc)) == 0);
		allocatedPso->AddRef();
		return handle;
	}

	AllocatedGraphicsPSO_D3D12* psoData;
	gpu::GraphicsPSOHandle const psoHandle = gpu::GraphicsPSOHandle(g_device->m_psoHandles.Alloc(psoData));
	psoData->Init(g_device->m_d3dDev, _desc, vsShader->m_byteCode, psShader->m_byteCode, _desc.m_vs, _desc.m_ps);
	g_device->m_psoCache.Insert(hash, gpu::GraphicsPSORef{ psoHandle });

	psShader->m_linkedPsos.PushBack(psoHandle);
	vsShader->m_linkedPsos.PushBack(psoHandle);

	return psoHandle;
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

	for (kt::Array<gpu::GraphicsPSOHandle>::Iterator it = shader->m_linkedPsos.Begin();
		 it != shader->m_linkedPsos.End();
		 /* */)
	{
		AllocatedGraphicsPSO_D3D12* psoData = g_device->m_psoHandles.Lookup(*it);
		if (!psoData)
		{
			it = shader->m_linkedPsos.EraseSwap(it);
			continue;
		}

		ShaderBytecode const& vsBytecode = g_device->m_shaderHandles.Lookup(psoData->m_vs)->m_byteCode;
		ShaderBytecode const& psBytecode = g_device->m_shaderHandles.Lookup(psoData->m_ps)->m_byteCode; 

		if (psoData->m_pso)
		{
			g_device->GetFrameResources()->m_deferredDeletions.PushBack(psoData->m_pso);
			psoData->m_pso = nullptr;
		}

		psoData->CreateD3DPSO(g_device->m_d3dDev, psoData->m_psoDesc, vsBytecode, psBytecode);
		++numPsos;
		++it;
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

void AddRef(gpu::BufferHandle _handle)
{
	AddRefImpl(_handle, g_device->m_bufferHandles);
}

void AddRef(gpu::TextureHandle _handle)
{
	AddRefImpl(_handle, g_device->m_textureHandles);
}

void AddRef(gpu::ShaderHandle _handle)
{
	AddRefImpl(_handle, g_device->m_shaderHandles);
}

void AddRef(gpu::GraphicsPSOHandle _handle)
{
	AddRefImpl(_handle, g_device->m_psoHandles);
}

void Release(gpu::BufferHandle _handle)
{
	ReleaseRefImpl(_handle, g_device->m_bufferHandles);
}

void Release(gpu::TextureHandle _handle)
{
	ReleaseRefImpl(_handle, g_device->m_textureHandles);
}

void Release(gpu::ShaderHandle _handle)
{
	ReleaseRefImpl(_handle, g_device->m_shaderHandles);
}

void Release(gpu::GraphicsPSOHandle _handle)
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


	AllocatedTexture_D3D12* backBuffer = m_textureHandles.Lookup(m_backBuffers[m_cpuFrameIdx]);

	ID3D12CommandAllocator* allocator = m_commandQueueManager.GraphicsQueue().AcquireAllocator();
	ID3D12CommandList* list;
	D3D_CHECK(g_device->m_d3dDev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&list)));
	ID3D12GraphicsCommandList* gfxList = (ID3D12GraphicsCommandList*)list;

	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = backBuffer->m_res;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gfxList->ResourceBarrier(1, &barrier);

		backBuffer->m_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	uint64_t const fence = m_commandQueueManager.GraphicsQueue().ExecuteCommandLists(kt::MakeSlice(list));
	m_commandQueueManager.GraphicsQueue().ReleaseAllocator(allocator, fence);
	gfxList->Release();
}

void Device_D3D12::EndFrame()
{
	AllocatedTexture_D3D12* backBuffer = m_textureHandles.Lookup(m_backBuffers[m_cpuFrameIdx]);

	ID3D12CommandAllocator* allocator = m_commandQueueManager.GraphicsQueue().AcquireAllocator();
	ID3D12CommandList* list;
	D3D_CHECK(g_device->m_d3dDev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&list)));
	ID3D12GraphicsCommandList* gfxList = (ID3D12GraphicsCommandList*)list;

	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = backBuffer->m_res;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		gfxList->ResourceBarrier(1, &barrier);

		backBuffer->m_state = D3D12_RESOURCE_STATE_PRESENT;
	}

	uint64_t const fence = m_commandQueueManager.GraphicsQueue().ExecuteCommandLists(kt::MakeSlice(list));
	m_commandQueueManager.GraphicsQueue().ReleaseAllocator(allocator, fence);
	gfxList->Release();

	m_descriptorcbvsrvuavRingBuffer.OnEndOfFrame(m_cpuFrameIdx);

	// Todo: vsync
	D3D_CHECK(m_swapChain->Present(0, 0));
	m_frameFences[m_cpuFrameIdx] = m_commandQueueManager.GraphicsQueue().LastPostedFenceValue();
	m_cpuFrameIdx = (m_cpuFrameIdx + 1) % gpu::c_maxBufferedFrames;
	++m_frameCounter;
}

void GetSwapchainDimensions(uint32_t& o_width, uint32_t& o_height)
{
	o_width = g_device->m_swapChainWidth;
	o_height = g_device->m_swapChainHeight;
}

gpu::Format BackbufferFormat()
{
	return g_device->m_textureHandles.Lookup(g_device->m_backBuffers[0])->m_desc.m_format;
}


void Device_D3D12::FrameResources::ClearOnBeginFrame()
{
	// Deferred deletions
	for (ID3D12DeviceChild* obj: m_deferredDeletions)
	{
		KT_ASSERT(obj);
		obj->Release();
	}

	m_deferredDeletions.Clear();

	m_uploadAllocator.ClearOnBeginFrame();
}


void EnumBufferHandles(kt::StaticFunction<void(gpu::BufferHandle), 32> const& _ftor)
{
	kt::VersionedHandlePool<AllocatedBuffer_D3D12>& handlePool = g_device->m_bufferHandles;

	for (uint32_t i = handlePool.FirstAllocatedIndex(); 
		 handlePool.IsIndexInUse(i); 
		 i = handlePool.NextAllocatedIndex(i))
	{
		_ftor(gpu::BufferHandle{ handlePool.HandleForIndex(i) });
	}
}


void EnumTextureHandles(kt::StaticFunction<void(gpu::TextureHandle), 32> const& _ftor)
{
	kt::VersionedHandlePool<AllocatedTexture_D3D12>& handlePool = g_device->m_textureHandles;

	for (uint32_t i = handlePool.FirstAllocatedIndex();
		 handlePool.IsIndexInUse(i);
		 i = handlePool.NextAllocatedIndex(i))
	{
		_ftor(gpu::TextureHandle{ handlePool.HandleForIndex(i) });
	}
}


bool GetBufferInfo(BufferHandle _handle, BufferDesc& o_desc, char const*& o_name)
{
	if (AllocatedBuffer_D3D12* buf = g_device->m_bufferHandles.Lookup(_handle))
	{
		o_name = buf->m_debugName.Data();
		o_desc = buf->m_desc;
		return true;
	}


	return false;
}



}