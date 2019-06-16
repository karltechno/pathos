#pragma once
#include "Types.h"
#include "GPUDevice.h"

namespace gpu
{


template <typename HandleT>
struct HandleRef
{
	using HandleType = HandleT;

	HandleRef() = default;

	~HandleRef()
	{
		Reset();
	}

	HandleRef(HandleType const& _handle)
	{
		Acquire(_handle);
	}

	HandleRef& operator=(HandleType const& _handle)
	{
		Acquire(_handle);
		return *this;
	}

	HandleRef(HandleType&& _other)
	{
		AcquireNoRef(_other);
		_other = HandleType{};
	}


	HandleRef& operator=(HandleType&& _other)
	{
		AcquireNoRef(_other);
		_other = HandleType{};
		return *this;
	}

	HandleRef(HandleRef const& _other)
	{
		Acquire(_other.Handle());
	}

	HandleRef(HandleRef&& _other)
	{
		AcquireNoRef(_other.Handle());
		_other.m_handle = HandleType{};
	}

	HandleRef& operator=(HandleRef&& _other)
	{
		AcquireNoRef(_other.Handle());
		_other.m_handle = HandleType{};
		return *this;
	}

	HandleRef& operator=(HandleRef const& _other)
	{
		Acquire(_other.m_handle);
		return *this;
	}

	void Acquire(HandleType _handle)
	{
		Reset();
		if (_handle.IsValid())
		{
			gpu::AddRef(_handle);
		}
		m_handle = _handle;
	}

	void AcquireNoRef(HandleType _handle)
	{
		Reset();
		m_handle = _handle;
	}

	void Reset()
	{
		if (m_handle.IsValid())
		{
			gpu::Release(m_handle);
		}
		m_handle = HandleType{};
	}

	bool IsValid() const
	{
		return m_handle.IsValid();
	}

	HandleType Handle() const
	{
		return m_handle;
	}

	operator HandleType() const
	{
		return m_handle;
	}

private:
	HandleType m_handle;
};

template<typename T>
bool operator==(HandleRef<T> const& _lhs, HandleRef<T> const& _rhs)
{
	return _lhs.Handle() == _rhs.Handle();
}

template<typename T>
bool operator!=(HandleRef<T> const& _lhs, HandleRef<T> const& _rhs)
{
	return _lhs.Handle() != _rhs.Handle();
}

using PSORef			= HandleRef<PSOHandle>;
using ShaderRef			= HandleRef<ShaderHandle>;
using ResourceRef		= HandleRef<ResourceHandle>;
using BufferRef			= HandleRef<BufferHandle>;
using TextureRef		= HandleRef<TextureHandle>;

}