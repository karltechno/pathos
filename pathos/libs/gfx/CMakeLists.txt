set(GFX_SOURCES
    "DebugRender.h"
    "DebugRender.cpp"
    "Camera.h"
    "Camera.cpp"
    "EnvMap.h"
    "EnvMap.cpp"
    "Material.h"
    "Material.cpp"
    "Model.h"
    "Model.cpp"
    "Primitive.h"
    "Primitive.cpp"
    "Texture.h"
    "Texture.cpp"
    "ResourceManager.h"
    "ResourceManager.cpp"
    "Scene.h"
    "Scene.cpp"
    "ShadowUtils.h"
    "ShadowUtils.cpp"
    "Utils.h"
)

add_pathos_lib(gfx "${GFX_SOURCES}")
target_link_libraries(gfx PUBLIC kt core gpu PRIVATE cgltf mikktspace stb)
target_include_directories(gfx PUBLIC ${PATHOS_SHADER_SRC})