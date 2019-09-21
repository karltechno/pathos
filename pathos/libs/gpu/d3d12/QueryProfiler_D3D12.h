#pragma once
#include <kt/kt.h>
#include <kt/Array.h>

#include "Types.h"

struct ID3D12QueryHeap;
struct ID3D12Resource;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

namespace gpu
{

struct QueryProfiler_D3D12
{
	static uint32_t const c_maxQueries = 1024;

	struct QueryData
	{
		uint32_t m_frameWriteNext = 0;
		uint32_t m_totalFramesWritten = 0;
	};

	void Init(ID3D12Device* _dev);
	void Shutdown();

	QueryIndex BeginQuery(ID3D12GraphicsCommandList* _list);
	void EndQuery(ID3D12GraphicsCommandList* _list, QueryIndex _queryIdx);

	void ClearFrame(uint32_t _frameIdx);
	void ResolveFrame(uint32_t _frameIdx);

	void ResolveQuery(QueryIndex _index, uint64_t* o_begin, uint64_t* o_end);

	ID3D12QueryHeap* m_queryHeap;
	ID3D12Resource* m_readbackRes;

	struct Time
	{
		uint64_t begin;
		uint64_t end;
	};

	struct FrameInfo
	{
		uint32_t m_numQueries = 0;
	} m_frames[gpu::c_maxBufferedFrames];

	Time m_times[c_maxQueries * 2];
	uint32_t m_lastResolvedFrame = UINT32_MAX;
};

}