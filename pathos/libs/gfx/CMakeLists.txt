set(GFX_SOURCES
    "Camera.h"
    "Camera.cpp"
    "Material.h"
    "Material.cpp"
    "Resources.h"
    "Resources.cpp"
    "Model.h"
    "Model.cpp"
    "Texture.h"
    "Texture.cpp"
    "Scene.h"
    "Scene.cpp"
)

add_pathos_lib(gfx "${GFX_SOURCES}")
target_link_libraries(gfx PUBLIC kt core gpu res PRIVATE cgltf mikktspace stb)
target_include_directories(gfx PUBLIC ${PATHOS_SHADER_SRC})