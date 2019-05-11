set(EDITOR_SOURCES
    "Editor.h"
    "Editor.cpp"
    "ImGuiHandler.h"
    "ImGuiHandler.cpp"
    "GPUWindows.h"
    "GPUWindows.cpp"
)

add_pathos_lib(editor "${EDITOR_SOURCES}")
target_link_libraries(editor kt core input gpu imgui)