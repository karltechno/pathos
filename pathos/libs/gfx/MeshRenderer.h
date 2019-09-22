#pragma once
#include <kt/Mat4.h>

#include <gpu/HandleRef.h>
#include <gpu/CommandContext.h>
#include <gpu/Types.h>
#include <shaderlib/CommonShared.h>
#include <shaderlib/CullingShared.h>

#include "ResourceManager.h"
#include "Utils.h"

namespace gfx
{

struct GPUCullingBuffers
{
	GPUCullingBuffers()
	{
		instanceCullingData.Init(gpu::BufferFlags::ShaderResource | gpu::BufferFlags::Dynamic, 4096, gpu::Format::Unknown, "Scratch Mesh/Instance Culling Buffer");
	}

	gfx::ResizableDynamicBufferT<uint32_t> instanceCullingData;
};

class MeshRenderer
{
public:
	MeshRenderer();

	void Submit(gfx::ResourceManager::MeshIdx _meshIdx, kt::Mat4 const& _mtx);

	void BuildMultiDrawBuffersCPU(gpu::cmd::Context* _ctx);

	void BuildMultiDrawBuffersGPU(gpu::cmd::Context* _ctx, GPUCullingBuffers& _scratchCullBuffers);

	void RenderInstances(gpu::cmd::Context* _ctx);

	void Clear();

	struct alignas(16) Matrix3x4
	{
		float data[3 * 4];
	};

private:

	kt::Array<gfx::ResourceManager::MeshIdx> m_meshes;
	kt::Array<Matrix3x4> m_transforms3x4;

	gfx::ResizableDynamicBufferT<gpu::IndexedDrawArguments> m_indirectArgsBuf;
	gfx::ResizableDynamicBufferT<shaderlib::InstanceData_Xform> m_instanceXformBuf;
	gfx::ResizableDynamicBufferT<uint32_t> m_instanceIdx_MeshIdx_Buf;

	uint32_t m_numSubmeshesSubmittedThisFrame = 0;

	uint32_t m_batchesBuiltThisFrame = 0;

	bool m_builtThisFrameOnGPU;
};

}