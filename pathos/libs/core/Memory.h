#pragma once
#include <kt/kt.h>
#include <kt/LinearAllocator.h>

namespace core
{

void InitThreadFrameAllocator(uint32_t _size);
void ShutdownThreadFrameAllocator();

kt::LinearAllocator* GetThreadFrameAllocator();

void ResetThreadFrameAllocator();
}