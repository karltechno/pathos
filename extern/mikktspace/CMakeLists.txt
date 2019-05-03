cmake_minimum_required(VERSION 3.8)

set(MIKKTSPACE_SOURCES
    "mikktspace.h"
    "mikktspace.cpp"
)

add_library(mikktspace STATIC ${MIKKTSPACE_SOURCES})
set_target_properties(mikktspace PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(mikktspace PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_definitions(mikktspace PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()