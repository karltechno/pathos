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

namespace cmd
{

enum class CommandListFlags_D3D12 : uint32_t
{
	Graphics = 0x1,
	Compute = 0x2,
	Copy = 0x4,

	DirectQueueFlags = Graphics | Compute | Copy,
	ComputeQueueFlags = Compute | Copy,
	CopyQueueFlags = Copy
};
KT_ENUM_CLASS_FLAG_OPERATORS(CommandListFlags_D3D12);

enum class DirtyStateFlags : uint32_t
{
	None			= 0x0,
	VertexBuffer	= 0x1,
	IndexBuffer		= 0x2,
	PrimitiveType	= 0x4,

	PipelineState	= 0x8,

	RenderTarget	= 0x10,
	DepthBuffer		= 0x20,

	All = 0xFFFFFFFF
};
KT_ENUM_CLASS_FLAG_OPERATORS(DirtyStateFlags);

enum class DirtyDescriptorFlags : uint8_t
{
	None	= 0x0,
	CBV		= 0x1,
	SRV		= 0x2,
	UAV		= 0x4,

	All = 0xFF
};
KT_ENUM_CLASS_FLAG_OPERATORS(DirtyDescriptorFlags);


struct CommandContext_D3D12
{
	CommandContext_D3D12(ContextType _type, Device_D3D12* _dev);
	~CommandContext_D3D12();

	void ApplyStateChanges(CommandListFlags_D3D12 _dispatchType);
	void MarkDirtyIfBound(gpu::BufferHandle _handle);

	Device_D3D12* m_device;

	D3D12_COMMAND_LIST_TYPE m_d3dType;

	ID3D12GraphicsCommandList* m_cmdList;
	ID3D12CommandAllocator* m_cmdAllocator;

	DirtyStateFlags m_dirtyFlags = DirtyStateFlags::All;
	DirtyDescriptorFlags m_dirtyDescriptors[gpu::c_numShaderSpaces];

	ContextType m_ctxType;
	CommandListFlags_D3D12 m_cmdListFlags;

	struct State
	{
		gpu::PrimitiveType m_primitive;
		gpu::BufferRef m_vertexStreams[gpu::c_maxVertexStreams];
		gpu::BufferRef m_indexBuffer;

		gpu::GraphicsPSORef m_graphicsPso;
		uint32_t m_numRenderTargets = 0;

		gpu::TextureRef m_depthBuffer;
		gpu::TextureRef m_renderTargets[gpu::c_maxRenderTargets];
	
		gpu::BufferRef m_cbvs[gpu::c_numShaderSpaces][gpu::c_cbvTableSize];		

		// TODO: make view handle otherwise this is awkward without base resource for tex/buff.
		gpu::TextureRef m_srvTex[gpu::c_numShaderSpaces][gpu::c_srvTableSize];
	} m_state;
};

}

}