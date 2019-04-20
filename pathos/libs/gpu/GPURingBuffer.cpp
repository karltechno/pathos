#include "GPURingBuffer.h"
#include <kt/Memory.h>

namespace gpu
{

void RingBuffer::Init(uint64_t _memStart, uint64_t _memSize, uint8_t* _cpuStart)
{
	m_memBegin = _memStart;
	m_cpuStart = _cpuStart;
	m_size = _memSize;
	m_head = 0;
	m_tail = 0;
}

uint64_t RingBuffer::Alloc(uint64_t _size, uint64_t _align, uint8_t** o_cpuPtr)
{
	uint64_t const offsetForAlignAtHead = kt::AlignUp(m_memBegin + m_head, _align) - (m_memBegin + m_head);
	
	if (m_head + offsetForAlignAtHead + _size <= m_size)
	{
		// Got contiguous space after head.
		uint64_t const offset = m_head + offsetForAlignAtHead;
		uint64_t const ret = m_memBegin + offset;
		m_head = offset + _size;

		if (o_cpuPtr)
		{
			*o_cpuPtr = m_cpuStart ? m_cpuStart + offset : nullptr;
		}
		return ret;
	}

	// Is there space wrapping to ring start?
	uint64_t const offsetForAlignAtBase = kt::AlignUp(m_memBegin, _align) - m_memBegin;

	if (offsetForAlignAtBase + _size >= m_tail)
	{
		// Nope!
		KT_ASSERT(false);
		if (o_cpuPtr)
		{
			*o_cpuPtr = nullptr;
		}
		return 0;
	}

	uint64_t const ret = m_memBegin + offsetForAlignAtBase;
	m_head = offsetForAlignAtBase + _size;
	if (o_cpuPtr)
	{
		*o_cpuPtr = m_cpuStart ? m_cpuStart + offsetForAlignAtBase : nullptr;
	}

	return ret;
}

void RingBuffer::Reset()
{
	m_head = m_tail = 0;
}

uint64_t RingBuffer::SizeLeft() const
{
	if (m_head >= m_tail)
	{
		return m_size - (m_head - m_tail) - 1;
	}
	else
	{
		return m_tail - m_head - 1;
	}
}

}