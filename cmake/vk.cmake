FetchContent_Declare(vk GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers.git" GIT_TAG "v1.3.280" GIT_SHALLOW ON GIT_SUBMODULES "" SOURCE_SUBDIR "disabled/")
FetchContent_Declare(vk_hpp GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Hpp.git" GIT_TAG "v1.3.280" GIT_SHALLOW ON GIT_SUBMODULES "" SOURCE_SUBDIR "disabled/")
FetchContent_Declare(vk_bootstrap GIT_REPOSITORY "https://github.com/charles-lunarg/vk-bootstrap.git" GIT_TAG "v1.3.280" GIT_SHALLOW ON)
FetchContent_MakeAvailable(vk vk_hpp vk_bootstrap)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    "${vk_SOURCE_DIR}/include/"
	"${vk_hpp_SOURCE_DIR}/"
    "${vk_bootstrap_SOURCE_DIR}/src/")
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    "VULKAN_DEBUG_MSG_UTILS" # enable vulkan debug layers
    "VULKAN_HPP_NO_CONSTRUCTORS"
    "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC"
    "VULKAN_HPP_NO_SPACESHIP_OPERATOR"
    "VULKAN_HPP_NO_TO_STRING")
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)