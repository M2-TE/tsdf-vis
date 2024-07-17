FetchContent_Declare(vma GIT_REPOSITORY "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git" GIT_TAG "v3.0.1" GIT_SHALLOW ON GIT_SUBMODULES "" SOURCE_SUBDIR "disabled/" SYSTEM)
FetchContent_Declare(vma_hpp GIT_REPOSITORY "https://github.com/YaaZ/VulkanMemoryAllocator-Hpp.git" GIT_TAG "v3.0.1-3" GIT_SHALLOW ON GIT_SUBMODULES "" SOURCE_SUBDIR "disabled/" SYSTEM)
FetchContent_MakeAvailable(vma vma_hpp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    "${vma_SOURCE_DIR}/include/"
	"${vma_hpp_SOURCE_DIR}/include/")
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    "VMA_DYNAMIC_VULKAN_FUNCTIONS" 
	"VMA_STATIC_VULKAN_FUNCTIONS=0")