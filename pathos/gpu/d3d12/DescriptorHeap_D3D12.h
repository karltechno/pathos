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

	D3D12_CPU_DESCRIPTOR_HANDLE IndexToCPUPtr(uint32_t _idx) const;
	D3D12_GPU_DESCRIPTOR_HANDLE IndexToGPUPtr(uint32_t _idx) const;

	uint32_t CPUPtrToIndex(D3D12_CPU_DESCRIPTOR_HANDLE _ptr) const;

	bool IsFrom(D3D12_CPU_DESCRIPTOR_HANDLE _ptr) const;

	ID3D12DescriptorHeap* D3DDescriptorHeap()
	{
		return m_heap;
	}

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

	D3D12_CPU_DESCRIPTOR_HANDLE AllocOne();
	void Free(D3D12_CPU_DESCRIPTOR_HANDLE _ptr);

private:
	kt::Array<uint32_t> m_freeList;

	DescriptorHeap_D3D12 m_heap;
};

class LinearDescriptorHeap_D3D12
{
public:
	void Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName);
	void Shutdown();

	void Alloc(uint32_t _num, D3D12_CPU_DESCRIPTOR_HANDLE& o_cpuBase, D3D12_GPU_DESCRIPTOR_HANDLE& o_gpuBase);

	void Clear();

	ID3D12DescriptorHeap* D3DDescriptorHeap()
	{
		return m_heap.D3DDescriptorHeap();
	}

private:
	DescriptorHeap_D3D12 m_heap;

	uint32_t m_numAllocated = 0;
};

}