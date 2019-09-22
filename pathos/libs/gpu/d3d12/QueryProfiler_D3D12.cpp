#include "QueryProfiler_D3D12.h"

#include <d3d12.h>

#include "Types.h"
#include "Utils_D3D12.h"
#include "GPUDevice_D3D12.h"

namespace gpu
{

void QueryProfiler_D3D12::Init(ID3D12Device* _dev)
{
	D3D12_QUERY_HEAP_DESC desc{};
	desc.Count = c_maxQueries * 2;
	desc.NodeMask = 0;
	desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	D3D_CHECK(_dev->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_queryHeap)));

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = gpu::c_maxBufferedFrames * c_maxQueries * sizeof(uint64_t) * 2; // 2 for begin/end.
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Alignment = 0;

	D3D12_HEAP_PROPERTIES const readbackHeap = { D3D12_HEAP_TYPE_READBACK, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
	D3D_CHECK(_dev->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_readbackRes)));
}

void QueryProfiler_D3D12::Shutdown()
{
	SafeReleaseDX(m_readbackRes);
	SafeReleaseDX(m_queryHeap);
}

QueryIndex QueryProfiler_D3D12::BeginQuery(ID3D12GraphicsCommandList* _list)
{
	uint32_t const frameIdx = gpu::CPUFrameIndexWrapped();
	FrameInfo& frame = m_frames[frameIdx];
	KT_ASSERT(frame.m_numQueries < c_maxQueries);
	uint32_t const idx = frame.m_numQueries++;
	_list->EndQuery(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, idx*2);
	return idx;
}

void QueryProfiler_D3D12::EndQuery(ID3D12GraphicsCommandList* _list, QueryIndex _queryIdx)
{
	uint32_t const frameIdx = gpu::CPUFrameIndexWrapped();
	FrameInfo& frame = m_frames[frameIdx];
	KT_ASSERT(frame.m_numQueries >= _queryIdx);
	KT_UNUSED(frame);

	uint32_t const queryIdxOffs = _queryIdx*2;
	_list->EndQuery(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, queryIdxOffs + 1);
	uint64_t const byteOffs = (queryIdxOffs + frameIdx * c_maxQueries * 2) * sizeof(uint64_t);
	_list->ResolveQueryData(m_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, queryIdxOffs, 2, m_readbackRes, byteOffs);
}

void QueryProfiler_D3D12::ClearFrame(uint32_t _frameIdx)
{
	m_frames[_frameIdx].m_numQueries = 0;
}

void QueryProfiler_D3D12::ResolveFrame(uint32_t _frameIdx)
{
	FrameInfo& frameInfo = m_frames[_frameIdx];

	if (frameInfo.m_numQueries == 0)
	{
		return;
	}

	uint64_t* data;

	D3D12_RANGE range;
	range.Begin = _frameIdx * 2 * c_maxQueries * sizeof(uint64_t);
	range.End = range.Begin + 2 * c_maxQueries * sizeof(uint64_t);

	m_readbackRes->Map(0, &range, (void**)&data);
	D3D12_RANGE const rangeEmpty{};
	KT_SCOPE_EXIT(m_readbackRes->Unmap(0, &rangeEmpty));

	//data += _frameIdx * 2 * c_maxQueries;

	for (uint32_t i = 0; i < frameInfo.m_numQueries; ++i)
	{
		Time& t = m_times[i];
		t.begin = data[i*2];
		t.end = data[i*2 + 1];
	}
}

void QueryProfiler_D3D12::ResolveQuery(QueryIndex _index, uint64_t* o_begin, uint64_t* o_end)
{
	KT_ASSERT(_index < c_maxQueries);
	*o_begin = m_times[_index].begin;
	*o_end = m_times[_index].end;
}

}