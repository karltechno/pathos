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

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap_D3D12::IndexToCPUPtr(uint32_t _idx) const
{
	KT_ASSERT(_idx < m_maxDescriptors);
	D3D12_CPU_DESCRIPTOR_HANDLE ret = m_heapStartCPU;
	ret.ptr += _idx * m_descriptorIncrementSize;
	return ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap_D3D12::IndexToGPUPtr(uint32_t _idx) const
{
	KT_ASSERT(_idx < m_maxDescriptors);
	D3D12_GPU_DESCRIPTOR_HANDLE ret = m_heapStartGPU;
	ret.ptr += _idx * m_descriptorIncrementSize;
	return ret;
}

uint32_t DescriptorHeap_D3D12::CPUPtrToIndex(D3D12_CPU_DESCRIPTOR_HANDLE _ptr) const
{
	KT_ASSERT(IsFrom(_ptr));
	return uint32_t((_ptr.ptr - m_heapStartCPU.ptr) / m_descriptorIncrementSize);
}

bool DescriptorHeap_D3D12::IsFrom(D3D12_CPU_DESCRIPTOR_HANDLE _ptr) const
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

D3D12_CPU_DESCRIPTOR_HANDLE FreeListDescriptorHeap_D3D12::AllocOne()
{
	KT_ASSERT(m_freeList.Size() != 0);
	D3D12_CPU_DESCRIPTOR_HANDLE ret = m_heap.IndexToCPUPtr(m_freeList.Back());
	m_freeList.PopBack();
	return ret;
}

void FreeListDescriptorHeap_D3D12::Free(D3D12_CPU_DESCRIPTOR_HANDLE _ptr)
{
	// Todo: GC?
	KT_ASSERT(m_heap.IsFrom(_ptr));
	KT_ASSERT(m_freeList.Size() < m_heap.MaxDescriptors());
	m_freeList.PushBack(m_heap.CPUPtrToIndex(_ptr));
}


void RingBufferDescriptorHeap_D3D12::Init(DescriptorHeap_D3D12* _baseHeap, uint64_t _beginOffsetInDescriptors, uint64_t _numDescriptors)
{
	uint64_t const byteBeginOffset = _beginOffsetInDescriptors * _baseHeap->m_descriptorIncrementSize;
	m_ringBuffer.Init(_baseHeap->m_heapStartGPU.ptr + byteBeginOffset, _numDescriptors * _baseHeap->m_descriptorIncrementSize, (uint8_t*)_baseHeap->m_heapStartCPU.ptr + byteBeginOffset);
	m_handleIncrement = _baseHeap->m_descriptorIncrementSize;
	for (uint64_t& i : m_endOfFrameHeads)
	{
		i = c_invalidEndOfFrameHead;
	}
}

bool RingBufferDescriptorHeap_D3D12::Alloc(uint32_t _numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE& o_cpuBase, D3D12_GPU_DESCRIPTOR_HANDLE& o_gpuBase)
{
	uint8_t* cpuptr;
	uint64_t gpuptr = m_ringBuffer.Alloc(_numDescriptors * m_handleIncrement, 1, &cpuptr);
	if (!gpuptr)
	{
		KT_ASSERT(false);
		return false;
	}
	
	o_cpuBase.ptr = SIZE_T(cpuptr);
	o_gpuBase.ptr = gpuptr;
	return true;
}

void RingBufferDescriptorHeap_D3D12::OnBeginFrame(uint32_t _frameIdx)
{
	if (m_endOfFrameHeads[_frameIdx] != c_invalidEndOfFrameHead)
	{
		m_ringBuffer.AdvanceTailToPreviousHead(m_endOfFrameHeads[_frameIdx]);
		m_endOfFrameHeads[_frameIdx] = c_invalidEndOfFrameHead;
	}
}

void RingBufferDescriptorHeap_D3D12::OnEndOfFrame(uint32_t _frameIdx)
{
	m_endOfFrameHeads[_frameIdx] = m_ringBuffer.CurrentHead();
}

}