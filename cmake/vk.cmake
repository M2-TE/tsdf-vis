FetchContent_Declare(vk GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers.git" GIT_TAG "v1.3.280" GIT_SHALLOW ON GIT_SUBMODULES "" SOURCE_SUBDIR "disabled/")
FetchContent_Declare(vk_hpp GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Hpp.git" GIT_TAG "v1.3.280" GIT_SHALLOW ON GIT_SUBMODULES "" SOURCE_SUBDIR "disabled/")
FetchContent_MakeAvailable(vk vk_hpp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    "${vk_SOURCE_DIR}/include/"
	"${vk_hpp_SOURCE_DIR}/")
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    "VULKAN_VALIDATION_LAYERS" # enable vulkan validation layers
    "VULKAN_HPP_NO_SETTERS"
    "VULKAN_HPP_NO_TO_STRING"
    "VULKAN_HPP_NO_CONSTRUCTORS"
    "VULKAN_HPP_NO_SMART_HANDLE"
    "VULKAN_HPP_NO_SPACESHIP_OPERATOR"
    "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC")