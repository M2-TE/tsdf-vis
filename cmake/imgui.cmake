FetchContent_Declare(imgui GIT_REPOSITORY "https://github.com/ocornut/imgui.git" GIT_TAG "v1.90.9-docking" GIT_SHALLOW ON SYSTEM)
FetchContent_MakeAvailable(imgui)
target_sources(${PROJECT_NAME} PRIVATE
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp")
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${imgui_SOURCE_DIR}/")
target_compile_definitions(${PROJECT_NAME} PRIVATE "IMGUI_IMPL_VULKAN_NO_PROTOTYPES")

