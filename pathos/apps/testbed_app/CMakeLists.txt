set(TESTBED_APP_SOURCES
    "TestbedApp.h"
    "TestbedApp.cpp"
)

add_pathos_app(testbed_app "${TESTBED_APP_SOURCES}")
target_link_libraries(testbed_app app)

set_property(TARGET testbed_app PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${PATHOS_ASSSET_DIR})
