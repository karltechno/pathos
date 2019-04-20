set(APP_SOURCES
    "App.h"
    "App.cpp"
    "Window.h"
    "ImGuiHandler.h"
    "ImGuiHandler.cpp"
)

if(WIN32)
    list(APPEND APP_SOURCES "Win/Window_Win.cpp")
endif()

add_pathos_lib(app "${APP_SOURCES}")
target_link_libraries(app core input gpu imgui)
