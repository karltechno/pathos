cmake_minimum_required(VERSION 3.8)

project(pathos CXX)

# Global project vars
set(PATHOS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PATHOS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/pathos)
set(PATHOS_SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/pathos/shaders)
set(PATHOS_EXTERN ${CMAKE_CURRENT_SOURCE_DIR}/extern)
set(PATHOS_ASSSET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include("cmake/pathos_projects.cmake")

if(MSVC)
    add_definitions(/Zi)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
endif()

add_subdirectory(extern)
add_subdirectory(pathos)