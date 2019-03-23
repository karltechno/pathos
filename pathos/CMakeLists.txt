cmake_minimum_required(VERSION 3.2)


# general pathos lib compile settings.
if(MSVC)
    add_compile_options(/W4 /WX -D_CRT_SECURE_NO_WARNINGS)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Werror)
    add_compile_options(-ffast-math)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(app)
add_subdirectory(editor_app)
add_subdirectory(input)
add_subdirectory(gpu)
# add_subdirectory(gfx)