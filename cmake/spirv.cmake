set(SPIRV_REFLECT_EXECUTABLE     OFF)
set(SPIRV_REFLECT_STATIC_LIB     ON)
set(SPIRV_REFLECT_BUILD_TESTS    OFF)
set(SPIRV_REFLECT_ENABLE_ASSERTS OFF)
set(SPIRV_REFLECT_ENABLE_ASAN    OFF)

FetchContent_Declare(spirv_reflect GIT_REPOSITORY "https://github.com/KhronosGroup/SPIRV-Reflect.git" GIT_TAG "vulkan-sdk-1.3.280.0" GIT_SHALLOW ON GIT_SUBMODULES "" SYSTEM)
FetchContent_MakeAvailable(spirv_reflect)
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-reflect-static)