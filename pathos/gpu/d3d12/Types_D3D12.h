#pragma once
#include <gpu/Types.h>

struct ID3D12Resource;

namespace gpu
{

struct RenderTarget_D3D12
{
	ID3D12Resource* m_resource = nullptr;

	gpu::CPUPtr m_rtv = { 0 };
	gpu::CPUPtr m_srv = { 0 };
};

}