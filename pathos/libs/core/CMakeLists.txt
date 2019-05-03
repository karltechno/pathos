cmake_minimum_required(VERSION 3.8)

set(CORE_SOURCES
	"CVar.h"
	"CVar.cpp" 
)

add_pathos_lib(core "${CORE_SOURCES}")
target_link_libraries(core kt imgui)