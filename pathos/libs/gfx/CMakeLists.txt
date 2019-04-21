set(GFX_SOURCES
    "Camera.h"
    "Camera.cpp"
)
add_pathos_lib(gfx "${GFX_SOURCES}")
target_link_libraries(gfx core gpu)