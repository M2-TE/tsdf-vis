set(SDL_TEST_LIBRARY OFF)
set(SDL_DISABLE_INSTALL ON)
set(SDL_DISABLE_UNINSTALL ON)
set(SDL_VULKAN ON)
set(SDL_DIRECTX OFF)
set(SDL_OPENG OFF)
set(SDL_OPENGLES OFF)
set(SDL_SHARED ON)
if (${SDL_SHARED})
    set(SDL_STATIC OFF)
else()
    set(SDL_STATIC ON)
endif()

FetchContent_Declare(SDL
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    GIT_TAG "d1739ce3a826bbae22f88d8fba70462499f3734c"
    GIT_SHALLOW OFF # disabled shallow because of git tag hash usage
    OVERRIDE_FIND_PACKAGE
    SYSTEM)
FetchContent_MakeAvailable(SDL)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${SDL_SOURCE_DIR}/include")
if (${SDL_SHARED})
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-shared)
    set_target_properties(SDL3-shared PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)
endif()