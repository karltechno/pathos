set(EDITOR_SOURCES
    "Editor.h"
    "Editor.cpp"
)

add_pathos_exe(editor "${EDITOR_SOURCES}")
target_link_libraries(editor app)