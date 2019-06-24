#pragma once
#include <gpu/Types.h>
#include <gpu/CommandContext.h>


namespace gfx
{

void CreateCubemapFromEquirect(gpu::cmd::Context* _cmd, char const* _equirectPath, gpu::ResourceHandle _outCubemap);
void CreateCubemapFromEquirect(gpu::cmd::Context* _cmd, gpu::ResourceHandle _inEquirect, gpu::ResourceHandle _outCubemap);

void BakeEnvMapGGX(gpu::cmd::Context* _cmd, gpu::ResourceHandle _inCubeMap, gpu::ResourceHandle _outGGXMap);
}