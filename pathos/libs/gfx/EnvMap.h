#pragma once
#include <gpu/Types.h>
#include <gpu/CommandContext.h>


namespace gfx
{

void CreateCubemapFromEquirect(char const* _equirectPath, gpu::ResourceHandle _outCubemap, gpu::cmd::Context* _cmd);
void CreateCubemapFromEquirect(gpu::ResourceHandle _inEquirect, gpu::ResourceHandle _outCubemap, gpu::cmd::Context* _cmd);

}