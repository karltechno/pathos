#pragma once
#include "Types.h"
#include "CommandContext.h"
#include "HandleRef.h"

#include <kt/Array.h>

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

	ViewPort		= 0x40,
	ScissorRect		= 0x80,

	All = 0xFFFFFFFF
};
KT_ENUM_CLASS_FLAG_OPERATORS(DirtyStateFlags);

struct Barrier_D3D12
{
	gpu::ResourceRef m_res;
	gpu::ResourceState m_newState;
	bool m_isUav = false;
};

struct CommandContext_D3D12
{
	CommandContext_D3D12(ContextType _type, Device_D3D12* _dev);
	~CommandContext_D3D12();

	void ApplyStateChanges(CommandListFlags_D3D12 _dispatchType);

	Device_D3D12* m_device;

	D3D12_COMMAND_LIST_TYPE m_d3dType;

	ID3D12GraphicsCommandList* m_cmdList;
	ID3D12CommandAllocator* m_cmdAllocator;

	DirtyStateFlags m_dirtyFlags = DirtyStateFlags::All;

	ContextType m_ctxType;
	CommandListFlags_D3D12 m_cmdListFlags;

	struct TextureWithArrayMipIdx
	{
		bool Equals(gpu::TextureHandle _h, uint32_t _arrayIdx, uint32_t _mipIdx) const
		{
			return m_tex.Handle() == _h && m_arrayIdx == _arrayIdx && m_mipIdx == _mipIdx;
		}

		gpu::TextureRef m_tex;
		uint32_t m_arrayIdx;
		uint32_t m_mipIdx;
	};

	struct State
	{
		gpu::PrimitiveType m_primitive = gpu::PrimitiveType::Num_PrimitiveType;
		gpu::BufferRef m_vertexStreams[gpu::c_maxVertexStreams];
		gpu::BufferRef m_indexBuffer;

		gpu::PSORef m_pso;
		uint32_t m_numRenderTargets = 0;

		TextureWithArrayMipIdx m_depthBuffer;
		gpu::TextureRef m_renderTargets[gpu::c_maxRenderTargets];

		gpu::Rect m_scissorRect;

		struct
		{
			gpu::Rect m_rect;
			float m_depthMin;
			float m_depthMax;
		} m_viewport;

		kt::InplaceArray<D3D12_RESOURCE_BARRIER, 16u> m_batchedBarriers;
	} m_state;

private:
	void ApplyGraphicsStateChanges();
};

}

}