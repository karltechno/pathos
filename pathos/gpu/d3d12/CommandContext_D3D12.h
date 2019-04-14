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

class CommandContext_D3D12
{
public:
	CommandContext_D3D12(D3D12_COMMAND_LIST_TYPE _type, Device_D3D12* _dev);
	~CommandContext_D3D12();

	void End();

	void SetVertexBuffer(uint32_t _streamIdx, gpu::BufferHandle _handle);
	void SetIndexBuffer(gpu::BufferHandle _handle);

	void DrawIndexedInstanced(gpu::PrimitiveType _prim, uint32_t _indexCount, uint32_t _instanceCount, uint32_t _startVtx, uint32_t _baseVtx, uint32_t _startInstance);

	void UpdateTransientBuffer(gpu::BufferHandle _handle, void const* _mem);

	void SetGraphicsPSO(gpu::GraphicsPSOHandle _pso);

	void ClearRenderTarget(gpu::TextureHandle _handle, float _color[4]);

private:
	void ApplyStateChanges(CommandListFlags _dispatchType);

	Device_D3D12* m_device;

	D3D12_COMMAND_LIST_TYPE m_type;

	//LinearDescriptorHeap_D3D12* m_descriptorAllocator;

	ID3D12GraphicsCommandList* m_cmdList;
	ID3D12CommandAllocator* m_cmdAllocator;

	enum DirtyStateFlags : uint32_t
	{
		VertexBuffer = 0x1,
		IndexBuffer = 0x2,
		PrimitiveType = 0x4,

		PipelineState = 0x8,
	};

	uint32_t m_dirtyFlags = 0xFFFFFFFF;

	struct State
	{
		gpu::PrimitiveType m_primitive;
		gpu::BufferRef m_vertexStreams[gpu::c_maxVertexStreams];
		gpu::BufferRef m_indexBuffer;

		gpu::GraphicsPSORef m_graphicsPso;
	} m_state;
};

}