cmake_minimum_required(VERSION 3.8)

macro(add_pathos_lib name sources)
    message("Adding pathos engine lib: ${name}")
    add_library(${name} STATIC ${sources})
    set_target_properties(${name} PROPERTIES FOLDER pathos_libs)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "src" FILES ${sources})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endmacro()

macro(add_pathos_app name sources)
    message("Adding pathos exe: ${name}")
    add_executable(${name} ${sources})
    set_target_properties(${name} PROPERTIES FOLDER pathos_apps)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "src" FILES ${sources})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    if(WIN32)
        add_custom_command(TARGET ${name} POST_BUILD 
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PATHOS_SRC}/libs/gpu/d3d12/WinPixEventRuntime/WinPixEventRuntime.dll"              
                $<TARGET_FILE_DIR:${name}>)
    endif()

endmacro()