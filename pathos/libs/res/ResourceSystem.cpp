#include <kt/Array.h>

#include "ResourceSystem.h"
#include "FolderWatcher.h"

#include <kt/HashMap.h>
#include <kt/FilePath.h>
#include <kt/Logging.h>
#include <kt/Serialization.h>
#include <kt/File.h>

namespace res
{

struct LoadedResource
{
	uint32_t m_typeTag;
	ResourceHandleBase m_handle;
};

struct ResourceContainer
{
	struct InternalResource
	{
		InternalResource() = default;

		~InternalResource()
		{
			kt::Free(m_path);
		}

		InternalResource(InternalResource&& _other)
			: m_path(_other.m_path)
			, m_ptr(_other.m_ptr)
		{
			_other.m_ptr = nullptr;
			_other.m_path = nullptr;
		}

		InternalResource& operator=(InternalResource&& _other)
		{
			m_path = _other.m_path;
			m_ptr = _other.m_ptr;
			_other.m_ptr = nullptr;
			_other.m_path = nullptr;
			return *this;
		}

		void SetPath(kt::StringView _view)
		{
			if (m_path)
			{
				kt::Free(m_path);
			}

			m_path = (char*)kt::Malloc(_view.Size() + 1);
			memcpy(m_path, _view.Data(), _view.Size());
			m_path[_view.Size()] = 0;
		}

		char* m_path = nullptr;
		void* m_ptr = nullptr;
	};

	ResourceContainer() = default;

	ResourceContainer(ResourceContainer&&) = default;
	ResourceContainer& operator=(ResourceContainer&&) = default;

	char const* m_resDebugName;

	res::IResourceHandler* m_handler;

	void Shutdown()
	{
		for (uint32_t i = m_handles.FirstAllocatedIndex();
			 m_handles.IsIndexInUse(i);
			 i = m_handles.NextAllocatedIndex(i))
		{
			InternalResource* res = m_handles.LookupAtIndex(i);
			KT_ASSERT(res);
			m_handler->Destroy(res->m_ptr);
			m_handles.Free(m_handles.HandleForIndex(i));
		}

		delete m_handler;
	}

	kt::VersionedHandlePool<InternalResource> m_handles;
};

struct Context
{
	kt::Array<ResourceContainer> m_resourceContainers;
	kt::HashMap<char const*, uint32_t> m_extensionToTypeTag;

	kt::HashMap<uint64_t, LoadedResource> m_loadedResourcesByAssetHash;

	res::FolderWatcher* m_folderWatcher = nullptr;
} s_ctx;

void Init()
{
	s_ctx.m_folderWatcher = res::CreateFolderWatcher(kt::FilePath("."));
}

void Shutdown()
{
	for (ResourceContainer& container : s_ctx.m_resourceContainers)
	{
		container.Shutdown();
	}

	s_ctx.m_resourceContainers.ClearAndFree();
	s_ctx.m_extensionToTypeTag.ClearAndFree();
	s_ctx.m_loadedResourcesByAssetHash.ClearAndFree();

	res::DestroyFolderWatcher(s_ctx.m_folderWatcher);
	s_ctx.m_folderWatcher = nullptr;
}


static kt::FilePath CanonicalizeAssetPath(char const* _path)
{
	// Todo: make relative to base asset dir
	kt::FilePath path(_path);
	char* p = path.DataMutable();
	while (*p)
	{
		*p = kt::ToLower(*p);
		++p;
	}
	return path;
}

static uint64_t HashAssetPath(kt::FilePath const& _path)
{
	return kt::StringHash64I(_path.Data());
}

void Tick()
{
	res::UpdateFolderWatcher(s_ctx.m_folderWatcher, [](char const* _path)
	{
		kt::FilePath const path = CanonicalizeAssetPath(_path);
		uint64_t const hash = HashAssetPath(path);
		kt::HashMap<uint64_t, LoadedResource>::Iterator it = s_ctx.m_loadedResourcesByAssetHash.Find(hash);
		if (it == s_ctx.m_loadedResourcesByAssetHash.End())
		{
			return;
		}

		Reload(it->m_val.m_handle, it->m_val.m_typeTag);
	});
}


res::ResourceHandleBase LoadResourceSync(char const* _path, uint32_t _typeTag)
{
	if (!kt::FileExists(_path))
	{
		KT_LOG_ERROR("Resource file: \"%s\" does not exist!", _path);
		return res::ResourceHandleBase{};
	}

	KT_ASSERT(_typeTag < s_ctx.m_resourceContainers.Size());
	ResourceContainer& container = s_ctx.m_resourceContainers[_typeTag];

	kt::FilePath const canonPath = CanonicalizeAssetPath(_path);
	uint64_t const assetHash = HashAssetPath(canonPath);

	kt::HashMap<uint64_t, LoadedResource>::Iterator it = s_ctx.m_loadedResourcesByAssetHash.Find(assetHash);
	if (it != s_ctx.m_loadedResourcesByAssetHash.End())
	{
		KT_ASSERT(it->m_val.m_typeTag == _typeTag);

		if (!container.m_handles.IsValid(it->m_val.m_handle))
		{
			s_ctx.m_loadedResourcesByAssetHash.Erase(it);
		}
		else
		{
			return it->m_val.m_handle;
		}
	}

	void* data = nullptr;
	KT_LOG_INFO("Creating resource from file \"%s\" (type: %s)", canonPath.Data(), container.m_resDebugName);

	if (!container.m_handler->CreateFromFile(_path, data))
	{
		KT_LOG_ERROR("Failed to create resource: %s", canonPath.Data());
		return ResourceHandleBase{};
	}

	ResourceContainer::InternalResource* internalRes;

	ResourceHandleBase const retHandle = ResourceHandleBase{ container.m_handles.Alloc(internalRes) };
	internalRes->m_ptr = data;
	internalRes->SetPath(canonPath.Data());

	s_ctx.m_loadedResourcesByAssetHash.Insert(assetHash, LoadedResource{ _typeTag, retHandle });

	return retHandle;
}

res::ResourceHandleBase CreateEmptyResource(char const* _path, void*& o_mem, uint32_t _typeTag, bool& o_resourceWasAlreadyCreated)
{
	KT_ASSERT(_typeTag < s_ctx.m_resourceContainers.Size());
	ResourceContainer& container = s_ctx.m_resourceContainers[_typeTag];

	kt::FilePath const canonPath = CanonicalizeAssetPath(_path);
	uint64_t const assetHash = HashAssetPath(canonPath);

	kt::HashMap<uint64_t, LoadedResource>::Iterator it = s_ctx.m_loadedResourcesByAssetHash.Find(assetHash);
	if (it != s_ctx.m_loadedResourcesByAssetHash.End())
	{
		KT_ASSERT(it->m_val.m_typeTag == _typeTag);

		ResourceContainer::InternalResource* foundRes = container.m_handles.Lookup(it->m_val.m_handle);

		if (!foundRes)
		{
			s_ctx.m_loadedResourcesByAssetHash.Erase(it);
		}
		else
		{
			o_resourceWasAlreadyCreated = true;
			o_mem = foundRes->m_ptr;
			return it->m_val.m_handle;
		}
	}

	o_resourceWasAlreadyCreated = false;

	void* data = nullptr;
	KT_LOG_INFO("Creating empty resource \"%s\" (type: %s)", canonPath.Data(), container.m_resDebugName);
	if (!container.m_handler->CreateEmpty(data))
	{
		return ResourceHandleBase{};
	}

	ResourceContainer::InternalResource* internalRes;

	ResourceHandleBase const retHandle = ResourceHandleBase{ container.m_handles.Alloc(internalRes) };
	internalRes->m_ptr = data;
	internalRes->SetPath(canonPath.Data());
	s_ctx.m_loadedResourcesByAssetHash.Insert(assetHash, LoadedResource{ _typeTag, retHandle });
	o_mem = data;
	return retHandle;
}

void* GetData(ResourceHandleBase _handle, uint32_t _typeTag)
{
	KT_ASSERT(_typeTag < s_ctx.m_resourceContainers.Size());
	ResourceContainer::InternalResource* res = s_ctx.m_resourceContainers[_typeTag].m_handles.Lookup(_handle);
	return res ? res->m_ptr : nullptr;
}


void RegisterResourceWithTypeTag(uint32_t _typeTag, char const* _debugName, kt::Slice<char const*> const& _extensions, IResourceHandler* _handler)
{
	KT_ASSERT(s_ctx.m_resourceContainers.Size() == _typeTag);
	ResourceContainer& container = s_ctx.m_resourceContainers.PushBack();
	container.m_handler = _handler;
	container.m_resDebugName = _debugName;

	container.m_handles.Init(kt::GetDefaultAllocator(), 256);

	for (char const* ext : _extensions)
	{
		KT_ASSERT(s_ctx.m_extensionToTypeTag.Find(ext) == s_ctx.m_extensionToTypeTag.End());
		s_ctx.m_extensionToTypeTag.Insert(ext, _typeTag);
	}
}

void Reload(ResourceHandleBase _handle, uint32_t _typeTag)
{
	KT_ASSERT(_typeTag < s_ctx.m_resourceContainers.Size());
	ResourceContainer& container = s_ctx.m_resourceContainers[_typeTag];
	if (container.m_handler->SupportsReload())
	{
		ResourceContainer::InternalResource* res = s_ctx.m_resourceContainers[_typeTag].m_handles.Lookup(_handle);
		if (res)
		{
			KT_LOG_INFO("Reloading resource: \"%s\"", res->m_path);
			container.m_handler->ReloadFromFile(res->m_path, res->m_ptr);
		}
	}
	else
	{
		KT_LOG_WARNING("Resource type \"%s\" does not support reloading.", container.m_resDebugName);
	}
}

}