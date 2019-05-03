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

bool GetBufferInfo(gpu::BufferHandle _handle, gpu::BufferDesc& o_desc, char const*& o_name);
bool GetTextureInfo(gpu::TextureHandle _handle, gpu::TextureDesc& o_desc, char const*& o_name);
bool GetShaderInfo(gpu::TextureHandle _handle, gpu::ShaderType& o_type, char const*& o_name);

void AddRef(gpu::BufferHandle _handle);
void AddRef(gpu::TextureHandle _handle);
void AddRef(gpu::ShaderHandle _handle);
void AddRef(gpu::GraphicsPSOHandle _handle);

void Release(gpu::BufferHandle _handle);
void Release(gpu::TextureHandle _handle);
void Release(gpu::ShaderHandle _handle);
void Release(gpu::GraphicsPSOHandle _handle);

// Debugging:
void EnumBufferHandles(kt::StaticFunction<void(gpu::BufferHandle), 32> const& _ftor);
void EnumTextureHandles(kt::StaticFunction<void(gpu::TextureHandle), 32> const& _ftor);
}

