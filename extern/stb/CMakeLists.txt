cmake_minimum_required(VERSION 3.8)

set(STB_SOURCES
    "stb_image_resize.h"
    "stb_image_resize.cpp"
    "stb_image.h"
    "stb_image.cpp"
)

add_library(stb STATIC ${STB_SOURCES})
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})