set(GFX_SOURCES
    "DebugRender.h"
    "DebugRender.cpp"
    "Camera.h"
    "Camera.cpp"
    "EnvMap.h"
    "EnvMap.cpp"
    "Material.h"
    "Material.cpp"
    "Resources.h"
    "Resources.cpp"
    "Model.h"
    "Model.cpp"
    "Primitive.h"
    "Primitive.cpp"
    "Texture.h"
    "Texture.cpp"
    "Scene.h"
    "Scene.cpp"
    "ShadowUtils.h"
    "ShadowUtils.cpp"
    "SharedResources.h"
    "SharedResources.cpp"
)

add_pathos_lib(gfx "${GFX_SOURCES}")
target_link_libraries(gfx PUBLIC kt core gpu res PRIVATE cgltf mikktspace stb)
target_include_directories(gfx PUBLIC ${PATHOS_SHADER_SRC})