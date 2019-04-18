cmake_minimum_required(VERSION 3.8)

set(GPU_SOURCES
	"CommandContext.h"
	"GPUDevice.h"
	"HandleRef.h"
	"Types.h"
	"Types.cpp"
	"FormatConversion.h"
	"GPURingBuffer.h"
	"GPURingBuffer.cpp"
)

if(WIN32)
	list(APPEND  GPU_SOURCES
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
	)
endif()

add_pathos_lib(gpu "${GPU_SOURCES}")

if(WIN32)
	target_link_libraries(gpu d3d12 dxgi dxguid)
endif()