#pragma once
#include <kt/kt.h>
#include <kt/Handles.h>

namespace kt
{
struct IReader;
}

namespace res
{

struct ResourceHandleBase : kt::VersionedHandle
{
	explicit ResourceHandleBase(kt::VersionedHandle _handle)
		: kt::VersionedHandle(_handle)
	{
	}

	ResourceHandleBase() = default;

	static uint32_t TypeTagHWM()
	{
		return s_nextTypeTag;
	}

protected:
	static uint32_t s_nextTypeTag;
};

template <typename ResourceT>
struct ResourceHandle : ResourceHandleBase
{
	static uint32_t s_typeTag;

	explicit ResourceHandle(kt::VersionedHandle _handle)
		: ResourceHandleBase(_handle)
	{}

	ResourceHandle() = default;

	static uint32_t TypeTag()
	{
		KT_ASSERT(s_handleTypeTag != 0xFFFFFFFF && 
				  "If you hit this you either passed a template type without an associated resource handler - or that handler hasn't been registered.");

		return s_handleTypeTag;
	}

	static void InitTypeTag()
	{
		KT_ASSERT(s_handleTypeTag == 0xFFFFFFFF);
		s_handleTypeTag = ResourceHandleBase::s_nextTypeTag++;
	}

private:
	static uint32_t s_handleTypeTag;
};

template <typename ResourceT>
uint32_t res::ResourceHandle<ResourceT>::s_handleTypeTag = 0xFFFFFFFF;

}