cmake_minimum_required(VERSION 3.8)

set(CGLTF_SOURCES
    "cgltf.h"
    "cgltf.cpp"
)

add_library(cgltf STATIC ${CGLTF_SOURCES})
target_include_directories(cgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_definitions(cgltf PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()