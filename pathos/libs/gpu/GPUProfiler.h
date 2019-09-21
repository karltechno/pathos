#pragma once

#include <kt/kt.h>

namespace gpu
{

namespace cmd { struct Context; }

namespace profiler
{

struct ResolvedTreeEntry
{
	char const* name;
	uint32_t colour;

	uint64_t beginTime;
	uint64_t endTime;

	uint32_t childLink = UINT32_MAX;
	uint32_t siblingLink = UINT32_MAX;
};

void SetEnabled(bool _enabled);

void BeginFrame(gpu::cmd::Context* _ctx, uint32_t _frameIdx);
void EndFrame(gpu::cmd::Context* _ctx, uint32_t _frameIdx);

void ResolveFrame(uint32_t _frameIdx);

void GetFrameTree(ResolvedTreeEntry const** _root, uint32_t* _numNodes);

void Begin(gpu::cmd::Context* _ctx, char const* _name, uint32_t _colour);
void End(gpu::cmd::Context* _ctx);

}

}