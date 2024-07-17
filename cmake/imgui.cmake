FetchContent_Declare(imgui GIT_REPOSITORY "https://github.com/ocornut/imgui.git" GIT_TAG "docking" GIT_SHALLOW ON SYSTEM)
FetchContent_MakeAvailable(imgui)
file(GLOB IMGUI_SOURCE_FILES CONFIGURE_DEPENDS "${imgui_SOURCE_DIR}/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE
    ${IMGUI_SOURCE_FILES}
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp")
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${imgui_SOURCE_DIR}/")
target_compile_definitions(${PROJECT_NAME} PRIVATE "IMGUI_IMPL_VULKAN_NO_PROTOTYPES")

