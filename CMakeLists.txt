cmake_minimum_required(VERSION 3.28)
project(tsdf-vis LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(_global)

# create and configure main executable
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/include/")
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})

# fetch project dependencies
include(FetchContent)
set(FETCHCONTENT_QUIET OFF) # enable git output for FetchContent steps
set(FETCHCONTENT_UPDATES_DISCONNECTED OFF) # speed up consecutive config runs
include(vk)
include(vma)
include(sdl) # todo: set constant git tag
include(fmt)
include(glm)
include(glsl)
include(imgui) # todo: set constant git tag
