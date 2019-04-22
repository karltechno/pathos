#include "FolderWatcher.h"
#include <kt/Array.h>
#include <kt/Logging.h>
#include <kt/HashMap.h>
#include <kt/Timer.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace res
{

static kt::Duration const s_watchExpiredDuration = kt::Duration::FromMilliseconds(250);

struct WatchedFileData
{
	kt::TimePoint m_timeSeen;
	bool m_processed = false;
};

struct FolderWatcher
{
#if KT_PLATFORM_WINDOWS
	using ChangedFileMap = kt::HashMap<kt::String1024, WatchedFileData>;


	~FolderWatcher()
	{
		if (m_handle != INVALID_HANDLE_VALUE)
		{
			::CancelIo(m_handle);
			DWORD numBytes;
			::GetOverlappedResult(m_handle, &m_overlapped, &numBytes, TRUE);
			::CloseHandle(m_handle);
		}
	}

	kt::FilePath m_watchingPath;
	kt::Array<uint8_t> m_buffer;
	OVERLAPPED m_overlapped;
	HANDLE m_handle;


	ChangedFileMap m_changedFiles;
#endif
};

#if KT_PLATFORM_WINDOWS
static void CALLBACK Win32WatchCB(DWORD dwErrorCode, DWORD dwNumberOfBytesTransferred, LPOVERLAPPED lpOverlapped);

bool DoNextWatch(FolderWatcher* _watcher)
{
	return ReadDirectoryChangesW(_watcher->m_handle, _watcher->m_buffer.Data(), _watcher->m_buffer.Size(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE, nullptr, &_watcher->m_overlapped, Win32WatchCB);
}

static void CALLBACK Win32WatchCB(DWORD dwErrorCode, DWORD dwNumberOfBytesTransferred, LPOVERLAPPED lpOverlapped)
{
	if (dwNumberOfBytesTransferred == 0 || dwErrorCode != 0)
	{
		return;
	}

	FolderWatcher* watcher = (FolderWatcher*)lpOverlapped->hEvent;

	if (!watcher)
	{
		return;
	}

	DWORD offset = 0;
	while (true)
	{
		FILE_NOTIFY_INFORMATION* notify = (FILE_NOTIFY_INFORMATION*)(watcher->m_buffer.Data() + offset);
		offset += notify->NextEntryOffset;
		char strBuff[1024];

		int res = ::WideCharToMultiByte(CP_UTF8, 0, notify->FileName, notify->FileNameLength / sizeof(WCHAR), strBuff, sizeof(strBuff), nullptr, nullptr);
		if (res <= 0 || res == sizeof(strBuff))
		{
			KT_LOG_ERROR("Failed to convert watched file %.*S to UTF-8.", notify->FileNameLength, notify->FileName);
			continue;
		}
		strBuff[res] = 0;

		// TODO: dynamic string.
		kt::String1024 str(strBuff);
		// TODO: Double lookup, need to improve hash map API.
		if (watcher->m_changedFiles.Find(str) == watcher->m_changedFiles.End())
		{
			watcher->m_changedFiles.Insert(str, WatchedFileData{ kt::TimePoint::Now(), false });
		}

		if (notify->NextEntryOffset == 0)
			break;

	}

	DoNextWatch(watcher);
}


#endif

FolderWatcher* CreateFolderWatcher(kt::FilePath const& _path)
{
#if KT_PLATFORM_WINDOWS
	HANDLE fileHandle = ::CreateFileA(_path.Data(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
										nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, 0);

	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		KT_LOG_ERROR("Failed to open directory: \"%s\" for change watching.", _path.Data());
		return nullptr;
	}

	FolderWatcher* watcher = new FolderWatcher;
	watcher->m_watchingPath = _path;
	watcher->m_buffer.Resize(1024 * 32);
	watcher->m_handle = fileHandle;
	watcher->m_overlapped.hEvent = watcher;
	DoNextWatch(watcher);
	return watcher;
#else
	return nullptr;
#endif
}


void DestroyFolderWatcher(FolderWatcher* _watcher)
{
	delete _watcher;
}

void UpdateFolderWatcher(FolderWatcher* _watcher, kt::StaticFunction<void(char const*), 32> const& _changeCb)
{
	KT_UNUSED2(_watcher, _changeCb);

	kt::TimePoint const timeNow = kt::TimePoint::Now();

	for (FolderWatcher::ChangedFileMap::Iterator it = _watcher->m_changedFiles.Begin();
		 it != _watcher->m_changedFiles.End();
		 /* */)
	{
		if (!it->m_val.m_processed)
		{
			_changeCb(it->m_key.Data());
			it->m_val.m_processed = true;
		}

		if (timeNow - it->m_val.m_timeSeen >= s_watchExpiredDuration)
		{
			it = _watcher->m_changedFiles.Erase(it);
		}
		else
		{
			++it;
		}
	}

	::MsgWaitForMultipleObjectsEx(0, nullptr, 00, QS_ALLINPUT, MWMO_ALERTABLE);
}

}