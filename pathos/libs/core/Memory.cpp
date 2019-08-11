#include "Memory.h"

#include <kt/LinearAllocator.h>
#include <kt/Memory.h>

namespace core
{
thread_local kt::LinearAllocator* tls_frameAllocator;

kt::LinearAllocator* GetThreadFrameAllocator()
{
	kt::LinearAllocator* allocator = tls_frameAllocator;
	KT_ASSERT(allocator);
	return allocator;
}

void InitThreadFrameAllocator(uint32_t _size)
{
	KT_ASSERT(!tls_frameAllocator);
	tls_frameAllocator = new kt::LinearAllocator(kt::GetDefaultAllocator(), _size);
}

void ShutdownThreadFrameAllocator()
{
	delete tls_frameAllocator;
}

void ResetThreadFrameAllocator()
{
	tls_frameAllocator->Reset();
}

}