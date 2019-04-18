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

gpu::BufferHandle CreateBuffer(gpu::BufferDesc const& _desc, void const* _initialData = nullptr, char const* _debugName = nullptr);
gpu::TextureHandle CreateTexture(gpu::TextureDesc const& _desc, void const* _initialData = nullptr, char const* _debugName = nullptr);

void GetSwapchainDimensions(uint32_t& o_width, uint32_t& o_height);

gpu::GraphicsPSOHandle CreateGraphicsPSO(gpu::GraphicsPSODesc const& _desc);

gpu::ShaderHandle CreateShader(gpu::ShaderType _type, gpu::ShaderBytecode const& _byteCode);

gpu::TextureHandle CurrentBackbuffer();
gpu::TextureHandle BackbufferDepth();

void AddRef(gpu::BufferHandle _handle);
void AddRef(gpu::TextureHandle _handle);
void AddRef(gpu::ShaderHandle _handle);
void AddRef(gpu::GraphicsPSOHandle _handle);

void Release(gpu::BufferHandle _handle);
void Release(gpu::TextureHandle _handle);
void Release(gpu::ShaderHandle _handle);
void Release(gpu::GraphicsPSOHandle _handle);

}

