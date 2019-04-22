set(RES_SOURCES
    "Resource.h"
    "Resource.cpp"
    "ResourceSystem.h"
    "ResourceSystem.cpp"
    "FolderWatcher.h"
    "FolderWatcher.cpp"
)

add_pathos_lib(res "${RES_SOURCES}")