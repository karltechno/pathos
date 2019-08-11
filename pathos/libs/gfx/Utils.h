#pragma once
#include <gpu/HandleRef.h>
#include <gpu/CommandContext.h>

namespace gfx
{

struct ResizableDynamicBuffer
{
	void Init(gpu::BufferFlags _flags, uint32_t _elemStride, uint32_t _size, gpu::Format _fmt = gpu::Format::Unknown, char const* _debugName = nullptr)
	{
		gpu::BufferDesc desc;
		desc.m_flags = _flags;
		desc.m_format = _fmt;
		desc.m_sizeInBytes = _size * _elemStride;
		desc.m_strideInBytes = _elemStride;
		m_buffer = gpu::CreateBuffer(desc, nullptr, _debugName);
		m_size = _size;
		m_stride = _elemStride;
		m_isTransient = !!(_flags & gpu::BufferFlags::Transient);
	}

	// Note: Doesn't copy data - add if necessary.
	void EnsureSize(uint32_t _size)
	{
		KT_ASSERT(m_buffer.IsValid());

		if (m_isTransient)
		{
			return;
		}

		if (m_size < _size)
		{
			uint32_t const newSize = kt::Max(_size + _size / 4, m_size + m_size / 2);
			m_size = newSize;
			char const* name;
			gpu::ResourceType ty;
			gpu::BufferDesc desc;
			gpu::GetResourceInfo(m_buffer, ty, &desc, nullptr, &name);
			desc.m_sizeInBytes = newSize * m_stride;
			m_buffer = gpu::CreateBuffer(desc, nullptr, name);
		}
	}

	void* BeginUpdate(gpu::cmd::Context* _ctx)
	{
		return BeginUpdate(_ctx, m_size);
	}

	void* BeginUpdate(gpu::cmd::Context* _ctx, uint32_t _numElements)
	{
		EnsureSize(_numElements);

		if (m_isTransient)
		{
			return gpu::cmd::BeginUpdateTransientBuffer(_ctx, m_buffer, _numElements * m_stride).Data();
		}
		else
		{
			return gpu::cmd::BeginUpdateDynamicBuffer(_ctx, m_buffer, _numElements * m_stride, 0).Data();
		}
	}

	void EndUpdate(gpu::cmd::Context* _ctx)
	{
		if (m_isTransient)
		{
			return gpu::cmd::EndUpdateDynamicBuffer(_ctx, m_buffer);
		}
		else
		{
			return gpu::cmd::EndUpdateDynamicBuffer(_ctx, m_buffer);
		}
	}

	void Update(gpu::cmd::Context* _ctx, void* _ptr, uint32_t _size)
	{
		KT_ASSERT(m_buffer.IsValid());

		if (!m_isTransient)
		{
			if (m_size < _size)
			{
				uint32_t const newSize = kt::Max(_size + _size / 4, m_size + m_size / 2);
				m_size = newSize;
				char const* name;
				gpu::ResourceType ty;
				gpu::BufferDesc desc;
				gpu::GetResourceInfo(m_buffer, ty, &desc, nullptr, &name);
				desc.m_sizeInBytes = newSize * m_stride;
				
				// Copy the data directly here.
				m_buffer = gpu::CreateBuffer(desc, _ptr, _size * m_stride, name);
				return;
			}
		}

		memcpy(BeginUpdate(_ctx, _size), _ptr, _size * m_stride);
		EndUpdateDynamicBuffer(_ctx, m_buffer);
	}

	gpu::BufferRef m_buffer;
	uint32_t m_size;
	uint32_t m_stride;
	bool m_isTransient = false;
};

template <typename T>
struct ResizableDynamicBufferT : private ResizableDynamicBuffer
{
	using ResizableDynamicBuffer::EnsureSize;
	using ResizableDynamicBuffer::EndUpdate;
	using ResizableDynamicBuffer::m_buffer;
	using ResizableDynamicBuffer::m_size;
	using ResizableDynamicBuffer::m_stride;
	using ResizableDynamicBuffer::m_isTransient;

	void Init(gpu::BufferFlags _flags, uint32_t _size, gpu::Format _fmt = gpu::Format::Unknown, char const* _debugName = nullptr)
	{
		ResizableDynamicBuffer::Init(_flags, sizeof(T), _size, _fmt, _debugName);
	}

	T* BeginUpdate(gpu::cmd::Context* _ctx)
	{
		return (T*)ResizableDynamicBuffer::BeginUpdate(_ctx, m_size);
	}

	T* BeginUpdate(gpu::cmd::Context* _ctx, uint32_t _numElements)
	{
		return (T*)ResizableDynamicBuffer::BeginUpdate(_ctx, _numElements);
	}

	void Update(gpu::cmd::Context* _ctx, T* _ptr, uint32_t _size)
	{
		ResizableDynamicBuffer::Update(_ctx, _ptr, _size);
	}
};

}