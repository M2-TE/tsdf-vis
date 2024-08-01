# SPIR-V for shader reflection
set(SPIRV_REFLECT_EXECUTABLE     OFF)
set(SPIRV_REFLECT_STATIC_LIB     ON)
set(SPIRV_REFLECT_BUILD_TESTS    OFF)
set(SPIRV_REFLECT_ENABLE_ASSERTS OFF)
set(SPIRV_REFLECT_ENABLE_ASAN    OFF)
FetchContent_Declare(spirv_reflect GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Reflect.git" GIT_TAG "vulkan-sdk-1.3.290.0" GIT_SHALLOW ON GIT_SUBMODULES "")
FetchContent_MakeAvailable(spirv_reflect)
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-reflect-static)
# SPIRV-Tools for glslang compiler
set(SPIRV_BUILD_FUZZER OFF)
set(SPIRV_COLOR_TERMINAL OFF)
set(SPIRV_SKIP_TESTS ON)
set(SPIRV_SKIP_EXECUTABLES ON)
set(SKIP_SPIRV_TOOLS_INSTALL ON)
set(SPIRV_USE_SANITIZER OFF)
set(SPIRV_WARN_EVERYTHING OFF)
set(SPIRV_WERROR OFF)
set(SPIRV_CHECK_CONTEXT OFF)
set(SPIRV_TOOLS_BUILD_STATIC ON)
FetchContent_Declare(SPIRV-Headers GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Headers.git" GIT_TAG "vulkan-sdk-1.3.290.0" GIT_SHALLOW ON)
FetchContent_Declare(spirv_tools GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Tools.git" GIT_TAG "vulkan-sdk-1.3.290.0" GIT_SHALLOW)
FetchContent_MakeAvailable(SPIRV-Headers spirv_tools)
# glslang for runtime/static shader compilation
set(ENABLE_OPT ON)
set(ENABLE_HLSL OFF)
set(ENABLE_GLSLANG_JS OFF)
set(ENABLE_GLSLANG_BINARIES ON) # enable/disable CLI GLSL compiler
set(ENABLE_SPVREMAPPER OFF)
set(GLSLANG_ENABLE_INSTALL OFF)
set(GLSLANG_TESTS OFF)
set(BUILD_EXTERNAL OFF)
FetchContent_Declare(glslang GIT_REPOSITORY "https://github.com/KhronosGroup/glslang.git" GIT_TAG "vulkan-sdk-1.3.290.0" GIT_SHALLOW ON)
FetchContent_MakeAvailable(glslang)
target_link_libraries(${PROJECT_NAME} PRIVATE glslang::glslang)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${glslang_SOURCE_DIR}/glslang/Public")
set(GLSLANG_BINARY_DIR "${glslang_BINARY_DIR}/StandAlone")

# compile shaders for now, TODO: simply validate them, they can be compiled at runtime
# todo: instead of just simple validation, maybe also do #include resolve
# todo: or maybe just do it at runtime..
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp")
foreach(GLSL_FILE ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME "${GLSL_FILE}" NAME)
    set(SPIRV_FILE "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        COMMENT "Compiling shader: ${FILE_NAME}"
        OUTPUT  "${SPIRV_FILE}"
        COMMAND "${GLSLANG_BINARY_DIR}/$<CONFIG>/glslang" -V "${GLSL_FILE}" -o "${SPIRV_FILE}"
        DEPENDS "${GLSL_FILE}" glslang-standalone)
    list(APPEND SPIRV_BINARY_FILES "${SPIRV_FILE}")
endforeach(GLSL_FILE)
FetchContent_Declare(cmrc GIT_REPOSITORY "https://github.com/vector-of-bool/cmrc.git" GIT_TAG "2.0.1" GIT_SHALLOW ON)
FetchContent_MakeAvailable(cmrc)
cmrc_add_resource_library(shaders ALIAS cmrc::shaders WHENCE "${CMAKE_CURRENT_BINARY_DIR}/shaders/" "${SPIRV_BINARY_FILES}")
target_link_libraries(${PROJECT_NAME} PRIVATE cmrc::shaders)