#pragma once
#include <kt/kt.h>

#include "Types.h"

namespace gpu
{

#if KT_PLATFORM_WINDOWS // TODO: Replace with D3D12 macro.
class CommandContext_D3D12;
using CommandContextImpl = CommandContext_D3D12;
#endif

enum class CommandListFlags : uint32_t
{
	Graphics	= 0x1,
	Compute		= 0x2,
	Copy		= 0x4,

	DirectQueueFlags	= Graphics | Compute | Copy,
	ComputeQueueFlags	= Compute | Copy,
	CopyQueueFlags		= Copy
};

KT_ENUM_CLASS_FLAG_OPERATORS(CommandListFlags);

struct CommandContext
{
	KT_NO_COPY(CommandContext);

	CommandContext(CommandListFlags _type, CommandContextImpl* _impl);
	~CommandContext();

	CommandContext(CommandContext&& _other)
		: m_impl(_other.m_impl)
		, m_typeFlags(_other.m_typeFlags)
	{
		_other.m_impl = nullptr;
	}

	CommandContext& operator=(CommandContext&& _other)
	{
		m_impl = _other.m_impl;
		m_typeFlags = _other.m_typeFlags;
		_other.m_impl = nullptr;
		return *this;
	}

	void SetGraphicsPSO(gpu::GraphicsPSOHandle _pso);

	void End();

	void SetVertexBuffer(uint32_t _streamIdx, gpu::BufferHandle _handle);
	void SetIndexBuffer(gpu::BufferHandle _handle);

	void DrawIndexedInstanced(gpu::PrimitiveType _prim, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startVtx, uint32_t _baseVtx, uint32_t _startInstance);

	void ClearRenderTarget(gpu::TextureHandle _handle, float _color[4]);

private:
	CommandContextImpl* m_impl;
	CommandListFlags m_typeFlags;
};

}