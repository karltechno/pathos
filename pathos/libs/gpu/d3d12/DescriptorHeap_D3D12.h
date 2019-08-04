#pragma once
#include <kt/Array.h>

#include <gpu/Types.h>
#include <gpu/GPURingBuffer.h>

#include <d3d12.h>
#include <stdint.h>

namespace gpu
{

struct DescriptorHeap_D3D12
{
	void Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName);
	void Shutdown();

	D3D12_CPU_DESCRIPTOR_HANDLE HandleBeginCPU() const { return m_heapStartCPU; }
	D3D12_GPU_DESCRIPTOR_HANDLE HandleBeginGPU() const { return m_heapStartGPU; }

	uint32_t MaxDescriptors() const { return m_maxDescriptors; }

	D3D12_CPU_DESCRIPTOR_HANDLE IndexToCPUPtr(uint32_t _idx) const;
	D3D12_GPU_DESCRIPTOR_HANDLE IndexToGPUPtr(uint32_t _idx) const;

	uint32_t CPUPtrToIndex(D3D12_CPU_DESCRIPTOR_HANDLE _ptr) const;

	bool IsFrom(D3D12_CPU_DESCRIPTOR_HANDLE _ptr) const;

	ID3D12DescriptorHeap* m_heap = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE m_type;

	D3D12_CPU_DESCRIPTOR_HANDLE m_heapStartCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGPU;

	uint32_t m_descriptorIncrementSize = 0;
	uint32_t m_maxDescriptors = 0;

	bool m_shaderVisible = false;
};

struct FreeListDescriptorHeap_D3D12
{
	void Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName);
	void Shutdown();

	D3D12_CPU_DESCRIPTOR_HANDLE AllocOne();
	void Free(D3D12_CPU_DESCRIPTOR_HANDLE _ptr);

	DescriptorHeap_D3D12 m_heap;

private:
	kt::Array<uint32_t> m_freeList;
};

struct RingBufferDescriptorHeap_D3D12
{
	void Init(DescriptorHeap_D3D12* _baseHeap, uint64_t _beginOffsetInDescriptors, uint64_t _numDescriptors);
	bool Alloc(uint32_t _numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE& o_cpuBase, D3D12_GPU_DESCRIPTOR_HANDLE& o_gpuBase);

	void OnBeginFrame(uint32_t _frameIdx);
	void OnEndOfFrame(uint32_t _frameIdx);

private:
	static uint64_t constexpr c_invalidEndOfFrameHead = UINT64_MAX;
	uint64_t m_handleIncrement = 0;
	gpu::RingBuffer m_ringBuffer;
	uint64_t m_endOfFrameHeads[gpu::c_maxBufferedFrames];
};

// TODO: This is just linear alloc until OOM, free is no-op. Improve me if/when this is a problem! (Buddy block allocator perhaps?)
struct PersistentDescriptorHeap_D3D12
{
	void Init(DescriptorHeap_D3D12* _baseHeap, uint64_t _beginOffsetInDescriptors, uint64_t _numDescriptors);

	bool Alloc(uint32_t _numDescriptors, uint32_t& o_idx);

	void Free(uint32_t _idx);

	DescriptorHeap_D3D12 const& BaseHeap() const { return *m_baseHeap; }

private:
	DescriptorHeap_D3D12* m_baseHeap = nullptr;
	uint64_t m_maxDescriptors = 0;
	uint64_t m_numAllocatedDescriptors = 0;
	uint64_t m_beginOffsetInDescriptors = 0;
};

}