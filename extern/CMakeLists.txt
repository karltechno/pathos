cmake_minimum_required(VERSION 3.2)

set_property(GLOBAL PROPERTY FOLDER extern)

add_subdirectory(kt)

set_target_properties(kt PROPERTIES FOLDER extern/kt)

if(TARGET kt_tests)
    set_target_properties(kt_tests PROPERTIES FOLDER extern/kt)
endif()

if(TARGET kt_bench)
    set_target_properties(kt_bench PROPERTIES FOLDER extern/kt)
endif()

# add_subdirectory(imgui)
# add_subdirectory(stb)