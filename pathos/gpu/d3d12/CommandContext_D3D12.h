#pragma once
#include "Types.h"
#include "CommandContext.h"
#include "HandleRef.h"

#include <d3d12.h>

struct ID3D12GraphicsCommandList;
struct ID3D12CommandAllocator;

namespace gpu
{

struct Device_D3D12;
class LinearDescriptorHeap_D3D12;

namespace cmd
{

enum class DirtyStateFlags : uint32_t
{
	None			= 0x0,
	VertexBuffer	= 0x1,
	IndexBuffer		= 0x2,
	PrimitiveType	= 0x4,

	PipelineState	= 0x8,

	RenderTarget	= 0x10,
	DepthBuffer		= 0x20,
};

KT_ENUM_CLASS_FLAG_OPERATORS(DirtyStateFlags);

struct CommandContext_D3D12
{
	CommandContext_D3D12(D3D12_COMMAND_LIST_TYPE _type, Device_D3D12* _dev);
	~CommandContext_D3D12();

	void ApplyStateChanges(CommandListFlags _dispatchType);

	Device_D3D12* m_device;

	D3D12_COMMAND_LIST_TYPE m_type;

	//LinearDescriptorHeap_D3D12* m_descriptorAllocator;

	ID3D12GraphicsCommandList* m_cmdList;
	ID3D12CommandAllocator* m_cmdAllocator;

	DirtyStateFlags m_dirtyFlags = DirtyStateFlags(0xFFFFFFFF);

	CommandListFlags m_cmdListFlags;

	struct State
	{
		gpu::PrimitiveType m_primitive;
		gpu::BufferRef m_vertexStreams[gpu::c_maxVertexStreams];
		gpu::BufferRef m_indexBuffer;

		gpu::GraphicsPSORef m_graphicsPso;
		uint32_t m_numRenderTargets = 0;

		gpu::TextureRef m_depthBuffer;
		gpu::TextureRef m_renderTargets[gpu::c_maxRenderTargets];
	
	} m_state;
};

}

}