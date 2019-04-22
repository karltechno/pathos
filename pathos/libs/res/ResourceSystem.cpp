#include <kt/Array.h>

#include "ResourceSystem.h"

#include <kt/HashMap.h>
#include <kt/FilePath.h>
#include <kt/Logging.h>
#include <kt/Serialization.h>

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

	res::LoaderFn m_createFn;
	res::DestroyFn m_destroyFn;
	res::ReloadFn m_reloadFn;

	void DestroyAll()
	{
		for (uint32_t i = m_handles.FirstAllocatedIndex();
			 m_handles.IsIndexInUse(i);
			 i = m_handles.NextAllocatedIndex(i))
		{
			InternalResource* res = m_handles.LookupAtIndex(i);
			KT_ASSERT(res);
			m_destroyFn(res->m_ptr);
			m_handles.Free(m_handles.HandleForIndex(i));
		}
	}



	kt::VersionedHandlePool<InternalResource> m_handles;
};

struct Context
{
	kt::Array<ResourceContainer> m_resourceContainers;
	kt::HashMap<char const*, uint32_t> m_extensionToTypeTag;

	kt::HashMap<uint64_t, LoadedResource> m_loadedResourcesByAssetHash;
} s_ctx;

void Init()
{

}

void Shutdown()
{
	for (ResourceContainer& container : s_ctx.m_resourceContainers)
	{
		container.DestroyAll();
	}

	s_ctx.m_resourceContainers.ClearAndFree();
	s_ctx.m_extensionToTypeTag.ClearAndFree();
	s_ctx.m_loadedResourcesByAssetHash.ClearAndFree();
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

res::ResourceHandleBase LoadResourceSync(char const* _path, uint32_t _typeTag)
{
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

	FILE* resFile = fopen(_path, "rb");
	if (!resFile)
	{
		KT_LOG_ERROR("Failed to open resource file \"%s\".", _path);
		return ResourceHandleBase{};
	}
	KT_SCOPE_EXIT(fclose(resFile));

	kt::FileReader reader(resFile);

	void* data = nullptr;
	if (!container.m_createFn(reader, reader.OriginalSize(), _path, data))
	{
		return ResourceHandleBase{};
	}

	ResourceContainer::InternalResource* internalRes;

	ResourceHandleBase const retHandle = ResourceHandleBase{ container.m_handles.Alloc(internalRes) };
	internalRes->m_ptr = data;
	internalRes->SetPath(canonPath.Data());

	s_ctx.m_loadedResourcesByAssetHash.Insert(assetHash, LoadedResource{ _typeTag, retHandle });

	return retHandle;
}

void* GetData(ResourceHandleBase _handle, uint32_t _typeTag)
{
	KT_ASSERT(_typeTag < s_ctx.m_resourceContainers.Size());
	ResourceContainer::InternalResource* res = s_ctx.m_resourceContainers[_typeTag].m_handles.Lookup(_handle);
	return res ? res->m_ptr : nullptr;
}


void RegisterResourceWithTypeTag(uint32_t _typeTag, char const* _debugName, kt::Slice<char const*> const& _extensions, LoaderFn&& _loader, DestroyFn&& _deleter, ReloadFn&& _reloader)
{
	KT_ASSERT(s_ctx.m_resourceContainers.Size() == _typeTag);
	ResourceContainer& container = s_ctx.m_resourceContainers.PushBack();
	container.m_createFn = _loader;
	container.m_destroyFn = _deleter;
	container.m_reloadFn = _reloader;
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
	if (container.m_reloadFn)
	{
		ResourceContainer::InternalResource* res = s_ctx.m_resourceContainers[_typeTag].m_handles.Lookup(_handle);
		if (res)
		{
			FILE* newFile = fopen(res->m_path, "rb");
			if (!newFile)
			{
				KT_LOG_ERROR("Failed to re-open file for asset reload: \"%s\"", res->m_path);
				return;
			}
			KT_SCOPE_EXIT(fclose(newFile));

			kt::FileReader reader(newFile);
			KT_LOG_INFO("Reloading resource: \"%s\"", res->m_path);
			container.m_reloadFn(reader, reader.OriginalSize(), res->m_ptr);
		}
	}
	else
	{
		KT_LOG_WARNING("Resource type \"%s\" does not support reloading.", container.m_resDebugName);
	}
}

}