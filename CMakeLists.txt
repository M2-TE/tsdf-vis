cmake_minimum_required(VERSION 3.22)
project(tsdf-vis LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(_global)

# create and configure main executable
file(GLOB_RECURSE CPP_SOURCE_FILES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(${PROJECT_NAME} ${CPP_SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(${PROJECT_NAME} PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/"
    CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/"
    CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/")

# fetch project dependencies
include(FetchContent)
set(FETCHCONTENT_QUIET OFF) # enable git output for FetchContent steps
set(FETCHCONTENT_UPDATES_DISCONNECTED ON) # speed up consecutive config runs
include(vk)
include(vma)
include(sdl)
include(fmt)
include(glm)
include(imgui)
include(shaders)
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Faster config after FetchContent has run once" FORCE)
