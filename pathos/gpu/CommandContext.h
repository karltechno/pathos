#pragma once
#include <kt/kt.h>

#include "Types.h"

namespace gpu
{


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

namespace cmd
{

#if KT_PLATFORM_WINDOWS // TODO: Replace with D3D12 macro.
struct CommandContext_D3D12;
using Context = CommandContext_D3D12;
#endif

void End(Context* _ctx);


void SetGraphicsPSO(Context* _ctx, gpu::GraphicsPSOHandle _pso);

void SetVertexBuffer(Context* _ctx, uint32_t _streamIdx, gpu::BufferHandle _handle);
void SetIndexBuffer(Context* _ctx, gpu::BufferHandle _handle);

void SetRenderTarget(Context* _ctx, uint32_t _idx, gpu::TextureHandle _handle);
void SetDepthBuffer(Context* _ctx, gpu::TextureHandle _handle);

void DrawIndexedInstanced(Context* _ctx, gpu::PrimitiveType _prim, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startVtx, uint32_t _baseVtx, uint32_t _startInstance);

void ClearRenderTarget(Context* _ctx, gpu::TextureHandle _handle, float const _color[4]);
void ClearDepth(Context* _ctx, gpu::TextureHandle _handle, float _depth); // TODO: Stencil

void UpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, void const* _mem);

void SetScissorRect(Context* _ctx, gpu::Rect const& _rect);
void SetViewport(Context* _ctx, gpu::Rect const& _rect, float _minDepth, float _maxDepth);

}

}