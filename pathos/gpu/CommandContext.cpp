#include "CommandContext.h"

#if KT_PLATFORM_WINDOWS
#include "d3d12/CommandContext_D3D12.h"
#endif

namespace gpu
{

#define ASSERT_CONTEXT(_flags) \
	KT_MACRO_BLOCK_BEGIN \
		KT_ASSERT(!!(m_typeFlags & (_flags))) \
	KT_MACRO_BLOCK_END

CommandContext::CommandContext(CommandListFlags _type, CommandContextImpl* _impl)
	: m_impl(_impl)
	, m_typeFlags(_type)
{

}

CommandContext::~CommandContext()
{
	delete m_impl;
}

void CommandContext::SetGraphicsPSO(gpu::GraphicsPSOHandle _pso)
{
	ASSERT_CONTEXT(CommandListFlags::Graphics);
	m_impl->SetGraphicsPSO(_pso);
}

void CommandContext::End()
{
	m_impl->End();
}

void CommandContext::SetVertexBuffer(uint32_t _streamIdx, gpu::BufferHandle _handle)
{
	ASSERT_CONTEXT(CommandListFlags::Graphics);
	m_impl->SetVertexBuffer(_streamIdx, _handle);
}

void CommandContext::SetIndexBuffer(gpu::BufferHandle _handle)
{
	ASSERT_CONTEXT(CommandListFlags::Graphics);
	m_impl->SetIndexBuffer(_handle);
}

void CommandContext::DrawIndexedInstanced(gpu::PrimitiveType _prim, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startVtx, uint32_t _baseVtx, uint32_t _startInstance)
{
	ASSERT_CONTEXT(CommandListFlags::Graphics);
	m_impl->DrawIndexedInstanced(_prim, _indexCount, _instanceCount, _startVtx, _baseVtx, _startInstance);
}

void CommandContext::ClearRenderTarget(gpu::TextureHandle _handle, float _color[4])
{
	ASSERT_CONTEXT(CommandListFlags::Graphics);
	m_impl->ClearRenderTarget(_handle, _color);
}

}