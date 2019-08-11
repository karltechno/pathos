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

cmd::Context* GetMainThreadCommandCtx();

gpu::BufferHandle CreateBuffer(gpu::BufferDesc const& _desc, void const* _initialData, uint32_t _initialDataSize, char const* _debugName);
gpu::BufferHandle CreateBuffer(gpu::BufferDesc const& _desc, void const* _initialData, char const* _debugName);

gpu::TextureHandle CreateTexture(gpu::TextureDesc const& _desc, void const* _initialData, char const* _debugName = nullptr);

void GetSwapchainDimensions(uint32_t& o_width, uint32_t& o_height);

gpu::PSOHandle CreateGraphicsPSO(gpu::GraphicsPSODesc const& _desc, char const* _debugName = nullptr);
gpu::PSOHandle CreateComputePSO(gpu::ShaderHandle _shader, char const* _debugName = nullptr);

gpu::PersistentDescriptorTableHandle CreatePersistentDescriptorTable(uint32_t _descriptorCount);
void SetPersistentTableSRV(gpu::PersistentDescriptorTableHandle _table, gpu::ResourceHandle _resource, uint32_t _idx);

gpu::ShaderHandle CreateShader(gpu::ShaderType _type, gpu::ShaderBytecode const& _byteCode, char const* _debugName = nullptr);
void ReloadShader(gpu::ShaderHandle _handle, gpu::ShaderBytecode const& _newBytecode);

void GenerateMips(gpu::cmd::Context* _ctx, gpu::ResourceHandle _handle);

gpu::TextureHandle CurrentBackbuffer();
gpu::TextureHandle BackbufferDepth();
gpu::Format	BackbufferFormat();
gpu::Format	BackbufferDepthFormat();

void SetVsyncEnabled(bool _vsync);

bool GetShaderInfo(gpu::ShaderHandle _handle, gpu::ShaderType& o_type, char const*& o_name);
bool GetResourceInfo(gpu::ResourceHandle _handle, gpu::ResourceType& _type, gpu::BufferDesc* o_bufferDesc = nullptr, gpu::TextureDesc* o_textureDesc = nullptr, char const** o_name = nullptr);
bool GetTextureInfo(gpu::TextureHandle _handle, gpu::TextureDesc& o_textureDesc);
bool GetBufferInfo(gpu::BufferHandle _handle, gpu::BufferDesc& o_bufferDesc);
uint32_t GetBufferNumElements(gpu::BufferHandle _handle);

void AddRef(gpu::ResourceHandle _handle);
void AddRef(gpu::ShaderHandle _handle);
void AddRef(gpu::PSOHandle _handle);
void AddRef(gpu::PersistentDescriptorTableHandle _handle);

void Release(gpu::ResourceHandle _handle);
void Release(gpu::ShaderHandle _handle);
void Release(gpu::PSOHandle _handle);
void Release(gpu::PersistentDescriptorTableHandle _handle);


// Debugging:
void EnumResourceHandles(kt::StaticFunction<void(gpu::ResourceHandle), 32> const& _ftor);
}

