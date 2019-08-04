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

void SetPSO(Context* _ctx, gpu::PSOHandle _pso);

void SetVertexBuffer(Context* _ctx, uint32_t _streamIdx, gpu::BufferHandle _handle);
void SetIndexBuffer(Context* _ctx, gpu::BufferHandle _handle);

void SetRenderTarget(Context* _ctx, uint32_t _idx, gpu::TextureHandle _handle);
void SetDepthBuffer(Context* _ctx, gpu::TextureHandle _handle, uint32_t _arrayIdx = 0, uint32_t _mipIdx = 0);

void SetComputeCBVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space);
void SetComputeUAVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space);
void SetComputeSRVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space);
void SetComputeSRVTable(Context* _ctx, gpu::PersistentDescriptorTableHandle _table, uint32_t _space);

void SetGraphicsCBVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space);
void SetGraphicsUAVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space);
void SetGraphicsSRVTable(Context* _ctx, kt::Slice<DescriptorData> const& _descriptors, uint32_t _space);
void SetGraphicsSRVTable(Context* _ctx, gpu::PersistentDescriptorTableHandle _table, uint32_t _space);

void ResourceBarrier(Context* _ctx, gpu::ResourceHandle _handle, gpu::ResourceState _newState);
void UAVBarrier(Context* _ctx, gpu::ResourceHandle _handle);
void FlushBarriers(Context* _ctx); 

void CopyResource(Context* _ctx, gpu::ResourceHandle _src, gpu::ResourceHandle _dest);

void DrawIndexedInstanced(Context* _ctx, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startIndex, uint32_t _baseVertex, uint32_t _startInstance);
void DrawInstanced(Context* _ctx, uint32_t _vertexCount, uint32_t _instanceCount, uint32_t _startVertex, uint32_t _startInstance);

void Dispatch(Context* _ctx, uint32_t _x, uint32_t _y, uint32_t _z);

void ClearRenderTarget(Context* _ctx, gpu::TextureHandle _handle, float const _color[4]);
void ClearDepth(Context* _ctx, gpu::TextureHandle _handle, float _depth, uint32_t _arrayIdx = 0, uint32_t _mipIdx = 0); // TODO: Stencil

// Merge functions?
void UpdateDynamicBuffer(Context* _ctx, gpu::BufferHandle _handle, void const* _mem, uint32_t _size, uint32_t _destOffset = 0);
kt::Slice<uint8_t> BeginUpdateDynamicBuffer(Context* _ctx, gpu::BufferHandle _handle, uint32_t _size, uint32_t _destOffset = 0);
void EndUpdateDynamicBuffer(Context* _ctx, gpu::BufferHandle _handle);

void UpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, void const* _mem, uint32_t _size);
kt::Slice<uint8_t> BeginUpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle, uint32_t _size);
void EndUpdateTransientBuffer(Context* _ctx, gpu::BufferHandle _handle);

void SetScissorRect(Context* _ctx, gpu::Rect const& _rect);
void SetViewport(Context* _ctx, gpu::Rect const& _rect, float _minDepth, float _maxDepth);

}

}