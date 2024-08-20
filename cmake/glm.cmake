set(GLM_DISABLE_AUTO_DETECTION  OFF)
set(GLM_ENABLE_LANG_EXTENSIONS  ${CMAKE_CXX_EXTENSIONS})
set(GLM_ENABLE_FAST_MATH        ${CROSS_PLATFORM_NONDETERMINISTIC})
set(GLM_ENABLE_CXX_20           ON)
set(GLM_FORCE_PURE              OFF)
set(GLM_ENABLE_SIMD_SSE         ${SDL_SSE})
set(GLM_ENABLE_SIMD_SSE2        ${SDL_SSE2})
set(GLM_ENABLE_SIMD_SSE3        ${SDL_SSE3})
set(GLM_ENABLE_SIMD_SSSE3       ${SDL_SSSE3})
set(GLM_ENABLE_SIMD_SSE4_1      ${SDL_SSE4_1})
set(GLM_ENABLE_SIMD_SSE4_2      ${SDL_SSE4_2})
set(GLM_ENABLE_SIMD_AVX         ${SDL_AVX})
set(GLM_ENABLE_SIMD_AVX2        ${SDL_AVX2})
set(GLM_ENABLE_SIMD_AVX512F     ${SDL_AVX512F})
FetchContent_Declare(glm GIT_REPOSITORY "https://github.com/g-truc/glm.git" GIT_TAG "1.0.1" GIT_SHALLOW ON)
FetchContent_MakeAvailable(glm)
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    "GLM_FORCE_DEPTH_ZERO_TO_ONE"
    "GLM_FORCE_ALIGNED_GENTYPES"
    "GLM_FORCE_INTRINSICS")
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)