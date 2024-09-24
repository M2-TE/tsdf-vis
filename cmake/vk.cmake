FetchContent_Declare(vulkan-headers
    GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Headers.git"
    GIT_TAG "v1.3.290"
    GIT_SHALLOW ON
    GIT_SUBMODULES ""
    SOURCE_SUBDIR "disabled"
    OVERRIDE_FIND_PACKAGE)
FetchContent_Declare(vulkan-hpp
    GIT_REPOSITORY "https://github.com/KhronosGroup/Vulkan-Hpp.git"
    GIT_TAG "v1.3.290"
    GIT_SHALLOW ON
    GIT_SUBMODULES ""
    SOURCE_SUBDIR "disabled"
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(vulkan-headers vulkan-hpp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    "${vulkan-headers_SOURCE_DIR}/include"
	"${vulkan-hpp_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    "VULKAN_VALIDATION_LAYERS" # enable vulkan validation layers
    "VULKAN_HPP_NO_SETTERS"
    "VULKAN_HPP_NO_TO_STRING"
    "VULKAN_HPP_NO_CONSTRUCTORS"
    "VULKAN_HPP_NO_SMART_HANDLE"
    "VULKAN_HPP_NO_SPACESHIP_OPERATOR"
    "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC")

# create vk_layer_settings.txt for validation layers
get_target_property(VALIDATION_LAYER_OVERRIDE_FOLDER ${PROJECT_NAME} RUNTIME_OUTPUT_DIRECTORY)
file(MAKE_DIRECTORY "${VALIDATION_LAYER_OVERRIDE_FOLDER}")
FILE(WRITE "${VALIDATION_LAYER_OVERRIDE_FOLDER}/vk_layer_settings.txt"
"# on by default:
khronos_validation.validate_core = true
khronos_validation.thread_safety = true
khronos_validation.validate_best_practices = false
khronos_validation.validate_best_practices_arm = true
khronos_validation.validate_best_practices_amd = true
khronos_validation.validate_best_practices_img = true
khronos_validation.validate_best_practices_nvidia = true
# off by default:
khronos_validation.validate_sync = true
khronos_validation.syncval_shader_accesses_heuristic = true
khronos_validation.gpuav_select_instrumented_shaders = true
khronos_validation.gpuav_debug_validate_instrumented_shaders = true
# etc:
khronos_validation.report_flags = info,warn,error")

# optimally, both of these should be enabled:
# khronos_validation.validate_best_practices = true
# khronos_validation.report_flags = info,warn,error,perf")