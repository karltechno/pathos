cmake_minimum_required(VERSION 3.11)

set(GPU_SOURCES
	"CommandContext.h"
	"GPUDevice.h"
	"HandleRef.h"
	"Types.h"
	"Types.cpp"
	"FormatConversion.h"
	"GPUProfiler.h"
	"GPUProfiler.cpp"
	"GPURingBuffer.h"
	"GPURingBuffer.cpp"
	"GPUHeap.h"
	"GPUHeap.cpp"
)

if(WIN32)
	list(APPEND GPU_SOURCES
		"d3d12/CommandContext_D3D12.h"
		"d3d12/CommandContext_D3D12.cpp"
		"d3d12/GPUDevice_D3D12.h" 
		"d3d12/GPUDevice_D3D12.cpp" 
		"d3d12/Utils_D3D12.h" 
		"d3d12/CommandQueue_D3D12.h" 
        "d3d12/CommandQueue_D3D12.cpp"
        "d3d12/DescriptorHeap_D3D12.cpp"
		"d3d12/DescriptorHeap_D3D12.h"   
		"d3d12/Types_D3D12.h"
		"d3d12/Types_D3D12.cpp"           
		"d3d12/d3dx12.h"
		"d3d12/GenMips.hlsli"    
		"d3d12/QueryProfiler_D3D12.h"
		"d3d12/QueryProfiler_D3D12.cpp"   
		"d3d12/WinPixEventRuntime/pix3.h"
		"d3d12/WinPixEventRuntime/pix3_win.h"
		"d3d12/WinPixEventRuntime/PIXEventsCommon.h"
		"d3d12/WinPixEventRuntime/PIXEventsGenerated.h"
	)

	set(COMPUTE_SOURCES 
		"d3d12/GenMipsLinear.hlsl"
		"d3d12/GenMipsSRGB.hlsl"
		"d3d12/GenMipsArrayLinear.hlsl"
		"d3d12/GenMipsArraySRGB.hlsl"
		"d3d12/GenMipsCubeLinear.hlsl"
		"d3d12/GenMipsCubeSRGB.hlsl"
	)

	set_property(SOURCE ${COMPUTE_SOURCES} PROPERTY VS_SHADER_TYPE Compute)
	set_property(SOURCE ${COMPUTE_SOURCES} PROPERTY VS_SHADER_ENTRY main)
	set_property(SOURCE ${COMPUTE_SOURCES} PROPERTY VS_SHADER_MODEL 5.1)
	set_property(SOURCE ${COMPUTE_SOURCES} PROPERTY VS_SHADER_OUTPUT_HEADER_FILE "%(RelativeDir)%(Filename).h")
	string(CONCAT hlsl_include "/I" ${PATHOS_SHADER_SRC})
	set_property(SOURCE ${COMPUTE_SOURCES} PROPERTY VS_SHADER_FLAGS ${hlsl_include})
	set_property(SOURCE ${COMPUTE_SOURCES} PROPERTY VS_SHADER_VARIABLE_NAME "g_%(Filename)")

	list(APPEND GPU_SOURCES ${COMPUTE_SOURCES})
endif()

add_pathos_lib(gpu "${GPU_SOURCES}")

target_link_libraries(gpu kt)

if(WIN32)
	target_link_libraries(gpu d3d12 dxgi dxguid "${CMAKE_CURRENT_SOURCE_DIR}/d3d12/WinPixEventRuntime/WinPixEventRuntime.lib")
endif()