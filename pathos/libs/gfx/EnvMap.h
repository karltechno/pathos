#pragma once
#include <gpu/Types.h>
#include <gpu/CommandContext.h>
#include <gpu/HandleRef.h>

#include "Primitive.h"


namespace gfx
{

struct Camera;

void CreateCubemapFromEquirect(gpu::cmd::Context* _cmd, char const* _equirectPath, gpu::ResourceHandle _outCubemap);
void CreateCubemapFromEquirect(gpu::cmd::Context* _cmd, gpu::ResourceHandle _inEquirect, gpu::ResourceHandle _outCubemap);

void BakeEnvMapGGX(gpu::cmd::Context* _cmd, gpu::ResourceHandle _inCubeMap, gpu::ResourceHandle _outGGXMap);

struct SkyBoxRenderer
{
	void Init(gpu::ResourceHandle _cubeMap);

	void Render(gpu::cmd::Context* _ctx, gfx::Camera const& _cam);

private:
	gpu::ResourceRef m_cubemap;
	gpu::PSORef m_skyBoxPso;

	gfx::PrimitiveGPUBuffers m_primGpuBuf;
};

}