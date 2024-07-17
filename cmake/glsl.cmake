find_package(Vulkan REQUIRED COMPONENTS glslangValidator)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders/")
file(GLOB_RECURSE GLSL_SOURCE_FILES CONFIGURE_DEPENDS 
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp")
foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME "${GLSL}" NAME)
    set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        COMMENT "Compiling shader: ${FILE_NAME}"
        OUTPUT  "${SPIRV}"
        COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V "${GLSL}" -o "${SPIRV}"
        DEPENDS "${GLSL}")
    list(APPEND SPIRV_BINARY_FILES "${SPIRV}")
endforeach(GLSL)

FetchContent_Declare(cmrc GIT_REPOSITORY "https://github.com/vector-of-bool/cmrc.git" GIT_TAG "2.0.1" GIT_SHALLOW ON SYSTEM)
FetchContent_MakeAvailable(cmrc)
cmrc_add_resource_library(shaders ALIAS cmrc::shaders WHENCE "${CMAKE_CURRENT_BINARY_DIR}/shaders/" "${SPIRV_BINARY_FILES}")
target_link_libraries(${PROJECT_NAME} PRIVATE cmrc::shaders)