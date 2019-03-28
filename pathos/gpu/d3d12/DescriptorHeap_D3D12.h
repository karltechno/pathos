#pragma once
#include <kt/Array.h>

#include <gpu/Types.h>

#include <d3d12.h>
#include <stdint.h>

namespace gpu
{

class DescriptorHeap_D3D12
{
public:
	void Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName);
	void Shutdown();

	D3D12_CPU_DESCRIPTOR_HANDLE HandleBeginCPU() { return m_heapStartCPU; }
	D3D12_GPU_DESCRIPTOR_HANDLE HandleBeginGPU() { return m_heapStartGPU; }

	uint32_t MaxDescriptors() const { return m_maxDescriptors; }

	gpu::CPUPtr IndexToCPUPtr(uint32_t _idx) const;
	gpu::GPUPtr IndexToGPUPtr(uint32_t _idx) const;

	uint32_t CPUPtrToIndex(gpu::CPUPtr _ptr) const;

	bool IsFrom(gpu::CPUPtr _ptr) const;

private:
	ID3D12DescriptorHeap* m_heap = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE m_type;

	D3D12_CPU_DESCRIPTOR_HANDLE m_heapStartCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGPU;

	uint32_t m_descriptorIncrementSize = 0;
	uint32_t m_maxDescriptors = 0;

	bool m_shaderVisible = false;
};

class FreeListDescriptorHeap_D3D12
{
public:
	void Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName);
	void Shutdown();

	gpu::CPUPtr AllocOne();
	void Free(gpu::CPUPtr _ptr);

private:
	kt::Array<uint32_t> m_freeList;

	DescriptorHeap_D3D12 m_heap;
};

class LinearDescriptorHeap_D3D12
{
public:
	void Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName);
	void Shutdown();

	void Alloc(uint32_t _num, gpu::CPUPtr& o_cpuBase, gpu::GPUPtr& o_gpuBase);

	void Clear();

private:
	DescriptorHeap_D3D12 m_heap;

	uint32_t m_numAllocated = 0;
};

}