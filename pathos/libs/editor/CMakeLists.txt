set(EDITOR_SOURCES
    "Editor.h"
    "Editor.cpp"
    "ImGuiHandler.h"
    "ImGuiHandler.cpp"
    "Windows/GPUWindows.h"
    "Windows/GPUWindows.cpp"
    "Windows/GFXSceneWindow.h"
    "Windows/GFXSceneWindow.cpp"
)

add_pathos_lib(editor "${EDITOR_SOURCES}")
target_link_libraries(editor kt core input gpu gfx imgui)