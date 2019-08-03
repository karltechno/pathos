cmake_minimum_required(VERSION 3.2)


# general pathos lib compile settings.
if(MSVC)
    add_compile_options(/W4 /WX -D_CRT_SECURE_NO_WARNINGS)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Werror)
    add_compile_options(-ffast-math)
endif()

# todo, don't apply this include to everything
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)

add_subdirectory(libs)
add_subdirectory(apps)