#pragma once
#include <kt/Slice.h>
#include <kt/StaticFunction.h>
#include "Resource.h"

namespace kt
{
struct IReader;
}

namespace res
{
using LoaderFn		= kt::StaticFunction<bool(kt::IReader& _reader, uint64_t _streamLen, char const* _filePath, void*& o_res), 32>;
using DestroyFn		= kt::StaticFunction<void(void* _oldRes), 32>;
using ReloadFn		= kt::StaticFunction<void(kt::IReader& _newRes, uint64_t _streamLen, void* _oldRes), 32>;

void Init();
void Shutdown();

ResourceHandleBase LoadResourceSync(char const* _path, uint32_t _typeTag);

template <typename T>
ResourceHandle<T> LoadResourceSync(char const* _path)
{
	return ResourceHandle<T>{LoadResourceSync(_path, ResourceHandle<T>::TypeTag())};
}

void* GetData(ResourceHandleBase _handle, uint32_t _typeTag);

template <typename T>
T* GetData(ResourceHandle<T> _handle)
{
	return (T*)GetData(_handle, ResourceHandle<T>::TypeTag());
}

void RegisterResourceWithTypeTag(uint32_t _typeTag, char const* _debugName, kt::Slice<char const*> const& _extensions, LoaderFn&& _loader, DestroyFn&& _deleter, ReloadFn&& _reloader);

template <typename T>
void RegisterResource(char const* _debugName, kt::Slice<char const*> const& _extensions, LoaderFn&& _loader, DestroyFn&& _deleter, ReloadFn&& _reloader = ReloadFn{})
{
	ResourceHandle<T>::InitTypeTag();
	RegisterResourceWithTypeTag(ResourceHandle<T>::TypeTag(), _debugName, _extensions, std::move(_loader), std::move(_deleter), std::move(_reloader));
}

// TODO: use a dir watcher
void Reload(ResourceHandleBase _handle, uint32_t _typeTag);

template <typename T>
void Reload(ResourceHandle<T> _handle)
{
	Reload(_handle, ResourceHandle<T>::TypeTag());
}

}