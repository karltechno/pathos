set(EDITOR_APP_SOURCES
    "EditorApp.h"
    "EditorApp.cpp"
)

add_pathos_exe(editor_app "${EDITOR_APP_SOURCES}")
target_link_libraries(editor_app app)

# todo, output to run_tree?
set_property(TARGET editor_app PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${PATHOS_RUN_TREE})
