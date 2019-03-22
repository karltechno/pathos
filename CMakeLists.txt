cmake_minimum_required(VERSION 3.2)

project(pathos CXX)

# Global project vars
set(PATHOS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(PATHOS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/pathos)
set(PATHOS_EXTERN ${CMAKE_CURRENT_SOURCE_DIR}/extern)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include("cmake/pathos_projects.cmake")

add_subdirectory(extern)
add_subdirectory(pathos)