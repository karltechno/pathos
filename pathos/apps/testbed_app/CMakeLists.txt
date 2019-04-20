set(TESTBED_APP_SOURCES
    "TestbedApp.h"
    "TestbedApp.cpp"
)

add_pathos_app(testbed_app "${TESTBED_APP_SOURCES}")
target_link_libraries(testbed_app app)

# todo, output to run_tree?
set_property(TARGET testbed_app PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${PATHOS_RUN_TREE})
