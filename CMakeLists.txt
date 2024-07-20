cmake_minimum_required(VERSION 3.22)
project(tsdf-vis LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(_global)

# create and configure main executable
add_executable(${PROJECT_NAME} 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ext.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/pipeline.cpp")
target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/include/")
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})

# fetch project dependencies
include(FetchContent)
set(FETCHCONTENT_QUIET OFF) # enable git output for FetchContent steps
set(FETCHCONTENT_UPDATES_DISCONNECTED ON) # speed up consecutive config runs
include(vk)
include(vma)
include(sdl) # todo: set constant git tag
include(fmt)
include(glm)
include(glsl)
include(spirv)
include(imgui) # todo: set constant git tag
