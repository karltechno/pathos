set(GPU_SOURCES
    "GPUDevice.cpp"
    "GPUDevice.h"
    "Types.h"
)

if(WIN32)
    list(APPEND GPU_SOURCES 
		"d3d12/GPUDevice_D3D12.h" 
		"d3d12/GPUDevice_D3D12.cpp" 
		"d3d12/D3D12_Utils.h" 
		"d3d12/CommandQueue.h" 
        "d3d12/CommandQueue.cpp"
        "d3d12/DescriptorHeap.cpp"
		"d3d12/DescriptorHeap.h"                        
	)

endif()

add_pathos_lib(gpu "${GPU_SOURCES}")

if(WIN32)
	target_link_libraries(gpu d3d12 dxgi)
endif()