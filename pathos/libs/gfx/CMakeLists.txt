set(GFX_SOURCES
    "Camera.h"
    "Camera.cpp"
    "Material.h"
    "Material.cpp"
    "Resources.h"
    "Resources.cpp"
)

add_pathos_lib(gfx "${GFX_SOURCES}")
target_link_libraries(gfx core gpu res)