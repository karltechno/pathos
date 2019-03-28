#pragma once
#include <stdint.h>

namespace gpu
{

struct Fence { uint64_t val; };

struct GPUPtr { uint64_t ptr; };
struct CPUPtr { uintptr_t ptr; };

struct ShaderBytecode
{
	void* m_data;
	size_t m_size;
};

}