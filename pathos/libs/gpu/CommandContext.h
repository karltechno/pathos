#pragma once
#include <kt/kt.h>
#include <kt/Slice.h>

#include "Types.h"

namespace gpu
{

namespace cmd
{

#if KT_PLATFORM_WINDOWS // TODO: Replace with D3D12 macro.
struct CommandContext_D3D12;
using Context = CommandContext_D3D12;
#endif

enum class ContextType
{
	Graphics,
	Compute,
	Copy,

	Num_ContextType
};

Context* Begin(ContextType _type);
void End(Context* _ctx);

ContextType GetContextType(Context* _ctx);

void SetGraphicsPSO(Context* _ctx, gpu::GraphicsPSOHandle _pso);

void SetVertexBuffer(Context* _ctx, uint32_t _streamIdx, gpu::BufferHandle _handle);
void SetIndexBuffer(Context* _ctx, gpu::BufferHandle _handle);

void SetRenderTarget(Context* _ctx, uint32_t _idx, gpu::TextureHandle _handle);
void SetDepthBuffer(Context* _ctx, gpu::TextureHandle _handle);

void SetConstantBuffer(Context* _ctx, gpu::BufferHandle _handle, uint32_t _idx, uint32_t _space);
void SetShaderResource(Context* _ctx, gpu::BufferHandle _handle, uint32_t _idx, uint32_t _space);
void SetShaderResource(Context* _ctx, gpu::TextureHandle _handle, uint32_t _idx, uint32_t _space);

void DrawIndexedInstanced(Context* _ctx, gpu::PrimitiveType _prim, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startIndex, uint32_t _baseVertex, uint32_t _startInstance);

void ClearRenderTarget(Context* _ctx, gpu::TextureHandle _handle, float const _color[4]);
void ClearDepth(Context* _ctx, gpu::TextureHandle _handle, float _depth); // TODO: Stencil

void UpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, void const* _mem, uint32_t _newSize = 0xFFFFFFFF);
kt::Slice<uint8_t> BeginUpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, uint32_t _newSize = 0xFFFFFFFF);
void EndUpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle);

void SetScissorRect(Context* _ctx, gpu::Rect const& _rect);
void SetViewport(Context* _ctx, gpu::Rect const& _rect, float _minDepth, float _maxDepth);

}

}