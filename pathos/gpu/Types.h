#pragma once
#include <stdint.h>

namespace gpu
{

struct Fence { uint64_t val; };

struct GPUPtr { uint64_t ptr; };
struct CPUPtr { uintptr_t ptr; };


}