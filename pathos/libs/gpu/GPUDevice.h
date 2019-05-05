#pragma once
#include <kt/kt.h>
#include <kt/StaticFunction.h>

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

gpu::ShaderHandle CreateShader(gpu::ShaderType _type, gpu::ShaderBytecode const& _byteCode, char const* _debugName = nullptr);
void ReloadShader(gpu::ShaderHandle _handle, gpu::ShaderBytecode const& _newBytecode);

gpu::TextureHandle CurrentBackbuffer();
gpu::TextureHandle BackbufferDepth();
gpu::Format	BackbufferFormat();

bool GetResourceInfo(gpu::ResourceHandle _handle, gpu::ResourceType& _type, gpu::BufferDesc* o_bufferDesc = nullptr, gpu::TextureDesc* o_textureDesc = nullptr, char const** o_name = nullptr);
bool GetShaderInfo(gpu::TextureHandle _handle, gpu::ShaderType& o_type, char const*& o_name);

void AddRef(gpu::ResourceHandle _handle);
void AddRef(gpu::ShaderHandle _handle);
void AddRef(gpu::GraphicsPSOHandle _handle);

void Release(gpu::ResourceHandle _handle);
void Release(gpu::ShaderHandle _handle);
void Release(gpu::GraphicsPSOHandle _handle);

// Debugging:
void EnumResourceHandles(kt::StaticFunction<void(gpu::ResourceHandle), 32> const& _ftor);
}

