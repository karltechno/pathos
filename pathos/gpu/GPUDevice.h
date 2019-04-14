#pragma once
#include <kt/kt.h>
#include "Types.h"
#include "CommandContext.h"

namespace gpu
{

bool Init(void* _nwh);
void Shutdown();

void BeginFrame();
void EndFrame();

gpu::BufferHandle CreateBuffer(gpu::BufferDesc const& _desc);
gpu::TextureHandle CreateTexture(gpu::TextureDesc const& _desc);

gpu::GraphicsPSOHandle CreateGraphicsPSO(gpu::GraphicsPSODesc const& _desc);

gpu::ShaderHandle CreateShader(gpu::ShaderType _type, gpu::ShaderBytecode const& _byteCode);

gpu::CommandContext CreateGraphicsContext();

gpu::TextureHandle CurrentBackbuffer();

void AddRef(gpu::BufferHandle _handle);
void AddRef(gpu::TextureHandle _handle);
void AddRef(gpu::ShaderHandle _handle);
void AddRef(gpu::GraphicsPSOHandle _handle);

void Release(gpu::BufferHandle _handle);
void Release(gpu::TextureHandle _handle);
void Release(gpu::ShaderHandle _handle);
void Release(gpu::GraphicsPSOHandle _handle);

}

