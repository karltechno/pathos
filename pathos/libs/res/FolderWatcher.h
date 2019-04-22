#pragma once
#include <kt/FilePath.h>
#include <kt/Platform.h>
#include <kt/StaticFunction.h>

namespace res
{

struct FolderWatcher;

FolderWatcher* CreateFolderWatcher(kt::FilePath const& _path);
void DestroyFolderWatcher(FolderWatcher* _watcher);

void UpdateFolderWatcher(FolderWatcher* _watcher, kt::StaticFunction<void(char const*), 32> const& _changeCb);

}