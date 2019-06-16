#include "SharedResources.h"
#include "Resources.h"

#include <res/ResourceSystem.h>
#include <gpu/GPUDevice.h>
#include <gfx/Texture.h>

namespace gfx
{

static SharedResources s_sharedResources;

void InitSharedResources()
{
    auto irradCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/BakeIrradianceMap.cs.cso");
    s_sharedResources.m_bakeIrradPso = gpu::CreateComputePSO(res::GetData(irradCs)->m_shader, "Bake_Irradiance");

    auto equics = res::LoadResourceSync<gfx::ShaderResource>("shaders/EquirectToCubemap.cs.cso");
	s_sharedResources.m_equiRectToCubePso = gpu::CreateComputePSO(res::GetData(equics)->m_shader, "Equirect_To_CubeMap");

	auto mipsCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/GenMipsLinear.cs.cso");
	auto mipsSrgbCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/GenMipsSRGB.cs.cso");

	s_sharedResources.m_genMipsPso[0] = gpu::CreateComputePSO(res::GetData(mipsCs)->m_shader, "GenMips_Linear");
	s_sharedResources.m_genMipsPso[1] = gpu::CreateComputePSO(res::GetData(mipsSrgbCs)->m_shader, "GenMips_sRGB");

	auto mipsArrayCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/GenMipsArrayLinear.cs.cso");
	auto mipsArraySrgbCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/GenMipsArraySRGB.cs.cso");

	s_sharedResources.m_genMipsArrayPso[0] = gpu::CreateComputePSO(res::GetData(mipsArrayCs)->m_shader, "GenMips_Array_Linear");
	s_sharedResources.m_genMipsArrayPso[1] = gpu::CreateComputePSO(res::GetData(mipsArraySrgbCs)->m_shader, "GenMips_Array_sRGB");

	auto mipsCubeCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/GenMipsCubeLinear.cs.cso");
	auto mipsCubeSrgbCs = res::LoadResourceSync<gfx::ShaderResource>("shaders/GenMipsCubeSRGB.cs.cso");

	s_sharedResources.m_genMipsCubePso[0] = gpu::CreateComputePSO(res::GetData(mipsCubeCs)->m_shader, "GenMips_Cube_Linear");
	s_sharedResources.m_genMipsCubePso[1] = gpu::CreateComputePSO(res::GetData(mipsCubeSrgbCs)->m_shader, "GenMips_Cube_sRGB");
}

void ShutdownSharedResources()
{
    s_sharedResources = SharedResources{};
}

SharedResources const& GetSharedResources()
{
    return s_sharedResources;
}

void GenerateMips(gpu::cmd::Context* _ctx, gpu::ResourceHandle _tex)
{
	gpu::ResourceType type;
	gpu::TextureDesc desc;
	gpu::GetResourceInfo(_tex, type, nullptr, &desc);
	KT_ASSERT(gpu::IsTexture(type));

	bool const srgb = gpu::IsSRGBFormat(desc.m_format);

	gpu::PSORef genMipsPso;
	uint32_t arraySlices = desc.m_arraySlices;

	if (type == gpu::ResourceType::TextureCube)
	{
		KT_ASSERT(desc.m_arraySlices == 1 && "Cube array mip gen not supported.");
		genMipsPso = s_sharedResources.m_genMipsCubePso[srgb];
		arraySlices = 6;
	}
	else if (desc.m_arraySlices > 1)
	{
		genMipsPso = s_sharedResources.m_genMipsArrayPso[srgb];
	}
	else
	{
		genMipsPso = s_sharedResources.m_genMipsPso[srgb];
	}


	struct MipCbuf
	{
		uint32_t srcMip;
		uint32_t numOutputMips;
		kt::Vec2 rcpTexelSize;
	} cbuf;

	uint32_t const mipsToGen = desc.m_mipLevels;

	uint32_t srcMip = 0;

	uint32_t destMipStart = 1;

	gpu::cmd::SetPSO(_ctx, genMipsPso);

	gpu::cmd::ResourceBarrier(_ctx, _tex, gpu::ResourceState::UnorderedAccess);

	gpu::DescriptorData srvDescriptor;
	srvDescriptor.Set(_tex);
	gpu::cmd::SetComputeSRVTable(_ctx, srvDescriptor, 0);


	while (destMipStart < mipsToGen)
	{
		uint32_t const c_maxMipsPerPass = 4;

		uint32_t const xDim = gfx::MipDimForLevel(desc.m_width, destMipStart);
		uint32_t const yDim = gfx::MipDimForLevel(desc.m_height, destMipStart);
		cbuf.rcpTexelSize.x = 1.0f / float(xDim);
		cbuf.rcpTexelSize.y = 1.0f / float(yDim);
		cbuf.srcMip = srcMip;
		cbuf.numOutputMips = kt::Min(c_maxMipsPerPass, mipsToGen - destMipStart);

		gpu::DescriptorData cbufDescriptor;
		gpu::DescriptorData uavDescriptor[c_maxMipsPerPass];

		cbufDescriptor.Set(&cbuf, sizeof(cbuf));

		for (uint32_t i = 0; i < cbuf.numOutputMips; ++i)
		{
			uavDescriptor[i].Set(_tex, destMipStart + i);
		}
		
		gpu::cmd::SetComputeUAVTable(_ctx, uavDescriptor, 0);
		gpu::cmd::SetComputeCBVTable(_ctx, cbufDescriptor, 0);
	
		uint32_t constexpr c_genMipDim = 8;
		gpu::cmd::Dispatch(_ctx, uint32_t(kt::AlignUp(xDim, c_genMipDim)) / c_genMipDim, uint32_t(kt::AlignUp(xDim, c_genMipDim)) / c_genMipDim, arraySlices);
		
		destMipStart += cbuf.numOutputMips;
		cbuf.srcMip += cbuf.numOutputMips;
		gpu::cmd::UAVBarrier(_ctx, _tex);
	}

	gpu::cmd::ResourceBarrier(_ctx, _tex, gpu::ResourceState::ShaderResource);
}

}