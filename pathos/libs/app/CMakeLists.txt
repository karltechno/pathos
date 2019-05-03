set(APP_SOURCES
    "App.h"
    "App.cpp"
    "Window.h"
    "ImGuiHandler.h"
    "ImGuiHandler.cpp"
    "CameraController.h"
    "CameraController.cpp"
)

if(WIN32)
    list(APPEND APP_SOURCES "Win/Window_Win.cpp")
endif()

add_pathos_lib(app "${APP_SOURCES}")
target_link_libraries(app kt core input gpu gfx imgui editor)
