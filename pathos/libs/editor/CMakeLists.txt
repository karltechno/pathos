set(EDITOR_SOURCES
    "Editor.h"
    "Editor.cpp"
)

add_pathos_lib(editor "${EDITOR_SOURCES}")
target_link_libraries(editor core input gpu imgui)