cmake_minimum_required(VERSION 3.8)

set_property(GLOBAL PROPERTY FOLDER extern)

add_subdirectory(kt)
add_subdirectory(imgui)
add_subdirectory(cgltf)
add_subdirectory(mikktspace)
add_subdirectory(stb)

set_target_properties(imgui PROPERTIES FOLDER extern)
set_target_properties(stb PROPERTIES FOLDER extern)
set_target_properties(cgltf PROPERTIES FOLDER extern)
set_target_properties(mikktspace PROPERTIES FOLDER extern)

set_target_properties(kt PROPERTIES FOLDER extern/kt)

if(TARGET kt_tests)
    set_target_properties(kt_tests PROPERTIES FOLDER extern/kt)
endif()

if(TARGET kt_bench)
    set_target_properties(kt_bench PROPERTIES FOLDER extern/kt)
endif()
