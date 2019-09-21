#include "GPUProfiler.h"
#include "Types.h"
#include "GPUDevice.h"
#include "CommandContext.h"

namespace gpu
{

namespace profiler
{

static uint32_t constexpr c_maxProfilesPerFrame = 1024;

struct TreeEntry
{
	char const* name;
	gpu::QueryIndex query;
	uint32_t colour;

	uint32_t parent = UINT32_MAX;

	uint32_t siblingNext = UINT32_MAX;

	uint32_t childLinkFirst = UINT32_MAX;
	uint32_t childLinkLast = UINT32_MAX;
};



struct FrameState
{
	TreeEntry m_tree[c_maxProfilesPerFrame];

	uint32_t m_numTreeNodes = 0;
	uint32_t m_curStackPos = UINT32_MAX;
};

struct State
{
	FrameState m_frames[gpu::c_maxBufferedFrames];

	ResolvedTreeEntry m_resolvedNodes[c_maxProfilesPerFrame];
	uint32_t m_numResolvedNodes = 0;

	bool m_enabled = true;
} s_state;



void SetEnabled(bool _enabled)
{
	s_state.m_enabled = _enabled;
}

void BeginFrame(gpu::cmd::Context* _ctx, uint32_t _frameIdx)
{
	FrameState& frame = s_state.m_frames[_frameIdx];
	frame.m_curStackPos = UINT32_MAX;
	frame.m_numTreeNodes = 0;

	profiler::Begin(_ctx, "Frame", 0xFF0000FF);
}

void EndFrame(gpu::cmd::Context* _ctx, uint32_t _frameIdx)
{
	KT_UNUSED(_frameIdx);
	profiler::End(_ctx);
}

void ResolveFrame(uint32_t _frameIdx)
{
	FrameState& frame = s_state.m_frames[_frameIdx];
	
	s_state.m_numResolvedNodes = 0;

	if (frame.m_numTreeNodes == 0)
	{
		return;
	}

	for (uint32_t i = 0; i < frame.m_numTreeNodes; ++i)
	{
		ResolvedTreeEntry& resolvedEntry = s_state.m_resolvedNodes[s_state.m_numResolvedNodes++];
		TreeEntry const& treeEntry = frame.m_tree[i];
		resolvedEntry = ResolvedTreeEntry{};

		resolvedEntry.colour = treeEntry.colour;
		resolvedEntry.name = treeEntry.name;
		resolvedEntry.childLink = treeEntry.childLinkFirst;
		resolvedEntry.siblingLink = treeEntry.siblingNext;
		gpu::ResolveQuery(treeEntry.query, &resolvedEntry.beginTime, &resolvedEntry.endTime);
	}
}

void GetFrameTree(ResolvedTreeEntry const** _root, uint32_t* _numNodes)
{
	*_root = s_state.m_resolvedNodes;
	*_numNodes = s_state.m_numResolvedNodes;
}

void Begin(gpu::cmd::Context* _ctx, char const* _name, uint32_t _colour)
{
	FrameState& frame = s_state.m_frames[gpu::CPUFrameIndexWrapped()];
	KT_ASSERT(frame.m_numTreeNodes < c_maxProfilesPerFrame);
	uint32_t const newNodeIdx = frame.m_numTreeNodes++;
	TreeEntry& entry = frame.m_tree[newNodeIdx];
	entry = TreeEntry{};

	entry.name = _name;
	entry.query = gpu::cmd::BeginQuery(_ctx);
	entry.colour = _colour;

	if (frame.m_curStackPos == UINT32_MAX)
	{
		frame.m_curStackPos = newNodeIdx;
		return;
	}

	// link it up.
	uint32_t const parentIdx = frame.m_curStackPos;
	entry.parent = parentIdx;
	TreeEntry& parent = frame.m_tree[parentIdx];
	
	if (parent.childLinkFirst == UINT32_MAX)
	{
		parent.childLinkFirst = parent.childLinkLast = newNodeIdx;
	}
	else
	{
		frame.m_tree[parent.childLinkLast].siblingNext = newNodeIdx;
		parent.childLinkLast = newNodeIdx;
	}

	frame.m_curStackPos = newNodeIdx;
}

void End(gpu::cmd::Context* _ctx)
{
	FrameState& frame = s_state.m_frames[gpu::CPUFrameIndexWrapped()];

	if (frame.m_curStackPos == UINT32_MAX)
	{
		KT_ASSERT(!"Mismatched begin/end");
		return;
	}

	gpu::cmd::EndQuery(_ctx, frame.m_tree[frame.m_curStackPos].query);
	frame.m_curStackPos = frame.m_tree[frame.m_curStackPos].parent;
}

}


}



