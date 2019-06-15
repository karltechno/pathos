#pragma once
#include <kt/Slice.h>
#include <kt/StaticFunction.h>
#include "Resource.h"

namespace kt
{
struct ISerializer;
struct IReader;
}

namespace res
{

struct IResourceHandler
{
	virtual ~IResourceHandler() = default;

	virtual bool CreateFromFile(char const* _filePath, void*& o_res) = 0;
	virtual bool CreateEmpty(void*& o_res) = 0;
	virtual void Destroy(void* _ptr) = 0;

	virtual void ReloadFromFile(char const* _filePath, void* _oldRes) { KT_UNUSED2(_filePath, _oldRes); }
	virtual bool SupportsReload() { return false; }
};

void Init();
void Shutdown();

void Tick();

ResourceHandleBase LoadResourceSync(char const* _path, uint32_t _typeTag);

template <typename T>
ResourceHandle<T> LoadResourceSync(char const* _path)
{
	return ResourceHandle<T>{LoadResourceSync(_path, ResourceHandle<T>::TypeTag())};
}

ResourceHandleBase CreateEmptyResource(char const* _path, void*& o_mem, uint32_t _typeTag, bool& o_resourceWasAlreadyCreated);

template <typename T>
ResourceHandle<T> CreateEmptyResource(char const* _path, T*& o_mem, bool& o_resourceWasAlreadyCreated)
{
	return ResourceHandle<T>{CreateEmptyResource(_path, (void*&)o_mem, ResourceHandle<T>::TypeTag(), o_resourceWasAlreadyCreated)};
}

void* GetData(ResourceHandleBase _handle, uint32_t _typeTag);

char const* GetResourcePath(ResourceHandleBase _handle, uint32_t _typeTag);

template <typename T>
char const* GetResourcePath(ResourceHandle<T> _handle)
{
	return GetResourcePath(_handle, ResourceHandle<T>::TypeTag());
}

template <typename T>
T* GetData(ResourceHandle<T> _handle)
{
	return (T*)GetData(_handle, ResourceHandle<T>::TypeTag());
}

void RegisterResourceWithTypeTag(uint32_t _typeTag, char const* _debugName, kt::Slice<char const*> const& _extensions, IResourceHandler* _handler);

template <typename T>
void RegisterResource(char const* _debugName, kt::Slice<char const*> const& _extensions, IResourceHandler* _handler)
{
	ResourceHandle<T>::InitTypeTag();
	RegisterResourceWithTypeTag(ResourceHandle<T>::TypeTag(), _debugName, _extensions, _handler);
}

void Reload(ResourceHandleBase _handle, uint32_t _typeTag);

template <typename T>
void Reload(ResourceHandle<T> _handle)
{
	Reload(_handle, ResourceHandle<T>::TypeTag());
}

void SerializeResourceHandle(kt::ISerializer* _s, ResourceHandleBase& _handle, uint32_t _typeTag);

template <typename T>
void SerializeResourceHandle(kt::ISerializer* _s, ResourceHandle<T>& _handle)
{
	SerializeResourceHandle(_s, _handle, ResourceHandle<T>::TypeTag());
}

}