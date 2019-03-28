#include <kt/Macros.h>

#include "DescriptorHeap_D3D12.h"
#include "Utils_D3D12.h"

namespace gpu
{


void DescriptorHeap_D3D12::Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName)
{
	KT_UNUSED(_debugName);
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = _shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NumDescriptors = _maxDescriptors;
	desc.Type = _ty;

	D3D_CHECK(_dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));

	m_maxDescriptors = _maxDescriptors;
	m_shaderVisible = _shaderVisible;
	m_type = _ty;
	m_descriptorIncrementSize = _dev->GetDescriptorHandleIncrementSize(_ty);
	m_heapStartCPU = m_heap->GetCPUDescriptorHandleForHeapStart();
	m_heapStartGPU = m_heap->GetGPUDescriptorHandleForHeapStart();
	D3D_SET_DEBUG_NAME(m_heap, _debugName);	
}

void DescriptorHeap_D3D12::Shutdown()
{
	SafeReleaseDX(m_heap);
}

gpu::CPUPtr DescriptorHeap_D3D12::IndexToCPUPtr(uint32_t _idx) const
{
	KT_ASSERT(_idx < m_maxDescriptors);
	return gpu::CPUPtr{ m_heapStartCPU.ptr + _idx * m_descriptorIncrementSize };
}

gpu::GPUPtr DescriptorHeap_D3D12::IndexToGPUPtr(uint32_t _idx) const
{
	KT_ASSERT(_idx < m_maxDescriptors);
	return gpu::GPUPtr{ m_heapStartCPU.ptr + _idx * m_descriptorIncrementSize };
}

uint32_t DescriptorHeap_D3D12::CPUPtrToIndex(gpu::CPUPtr _ptr) const
{
	KT_ASSERT(IsFrom(_ptr));
	return uint32_t((_ptr.ptr - m_heapStartCPU.ptr) / m_descriptorIncrementSize);
}

bool DescriptorHeap_D3D12::IsFrom(gpu::CPUPtr _ptr) const
{
	return _ptr.ptr > m_heapStartCPU.ptr && _ptr.ptr < (m_heapStartCPU.ptr + m_maxDescriptors * m_descriptorIncrementSize);
}

void FreeListDescriptorHeap_D3D12::Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName)
{
	m_heap.Init(_dev, _ty, _maxDescriptors, _shaderVisible, _debugName);
	uint32_t* freeList = m_freeList.PushBack_Raw(_maxDescriptors);
	for (uint32_t i = 0; i < _maxDescriptors; ++i)
	{
		freeList[i] = i;
	}
}

void FreeListDescriptorHeap_D3D12::Shutdown()
{
	m_heap.Shutdown();
	m_freeList.ClearAndFree();
}

gpu::CPUPtr FreeListDescriptorHeap_D3D12::AllocOne()
{
	KT_ASSERT(m_freeList.Size() != 0);

	gpu::CPUPtr ptr = { m_heap.IndexToCPUPtr(m_freeList.Back()) };
	m_freeList.PopBack();
	return ptr;
}

void FreeListDescriptorHeap_D3D12::Free(gpu::CPUPtr _ptr)
{
	// Todo: GC?
	KT_ASSERT(m_heap.IsFrom(_ptr));
	KT_ASSERT(m_freeList.Size() < m_heap.MaxDescriptors());
	m_freeList.PushBack(m_heap.CPUPtrToIndex(_ptr));
}

void LinearDescriptorHeap_D3D12::Init(ID3D12Device* _dev, D3D12_DESCRIPTOR_HEAP_TYPE _ty, uint32_t _maxDescriptors, bool _shaderVisible, char const* _debugName)
{
	m_heap.Init(_dev, _ty, _maxDescriptors, _shaderVisible, _debugName);
	m_numAllocated = 0;
}

void LinearDescriptorHeap_D3D12::Shutdown()
{
	m_heap.Shutdown();
	m_numAllocated = 0;
}

void LinearDescriptorHeap_D3D12::Alloc(uint32_t _num, gpu::CPUPtr& o_cpuBase, gpu::GPUPtr& o_gpuBase)
{
	KT_ASSERT(m_numAllocated + _num <= m_heap.MaxDescriptors());
	o_cpuBase = m_heap.IndexToCPUPtr(m_numAllocated);
	o_gpuBase = m_heap.IndexToGPUPtr(m_numAllocated);
	m_numAllocated += _num;
}

void LinearDescriptorHeap_D3D12::Clear()
{
	m_numAllocated = 0;
}

}