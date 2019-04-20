#pragma once
#include <kt/kt.h>

namespace gpu
{

struct RingBuffer
{
	void Init(uint64_t _memStart, uint64_t _memSize, uint8_t* _cpuStart);

	uint64_t Alloc(uint64_t _size, uint64_t _align, uint8_t** o_cpuPtr = nullptr);

	uint64_t CurrentHead() const
	{
		return m_head;
	}

	void AdvanceTailToPreviousHead(uint64_t _previousHead)
	{
		m_tail = _previousHead;
	}

	void Reset();

	uint64_t SizeLeft() const;

private:
	uint64_t m_memBegin = 0;
	uint8_t* m_cpuStart = nullptr;
	uint64_t m_size = 0;

	uint64_t m_head = 0;
	uint64_t m_tail = 0;
};

}