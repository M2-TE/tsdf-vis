#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>

struct Window {
    static constexpr bool using_debug_msg() {
#       ifdef VULKAN_DEBUG_MSG_UTILS
            return true;
#       else
            return false;
#       endif
    }
    auto init(int width, int height) -> std::vector<const char*> {
        // SDL: init video subsystem for render surfaces
        if (SDL_InitSubSystem(SDL_INIT_VIDEO)) fmt::println("{}", SDL_GetError());
        
        // SDL: create window
        _p_window = SDL_CreateWindow(_name.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (_p_window == nullptr) fmt::println("{}", SDL_GetError());
        
        // SDL: query required instance extensions
        uint32_t n_extensions;
        const char* const* p_extensions = SDL_Vulkan_GetInstanceExtensions(&n_extensions);
        if (p_extensions == nullptr) fmt::println("{}", SDL_GetError());
        // convert to c++ vector
        std::vector<const char*> extensions(n_extensions);
        for (uint32_t i = 0; i < n_extensions; i++) extensions[i] = p_extensions[i];
        return extensions;
    }
    void create_surface(vk::Instance instance, vk::DebugUtilsMessengerEXT msg) {
        // SDL: create surface
        VkSurfaceKHR surfaceTemp;
        if (SDL_Vulkan_CreateSurface(_p_window, instance, nullptr, &surfaceTemp)) fmt::println("{}", SDL_GetError());
        _surface = surfaceTemp;
        _debug_msg = msg;
        
        fmt::println("Using vulkan validation layers: {}", using_debug_msg());
    }
    void destroy(vk::Instance instance) {
        instance.destroySurfaceKHR(_surface);
        if (using_debug_msg()) {
            instance.destroyDebugUtilsMessengerEXT(_debug_msg);
        }
        SDL_Quit();
    }
    void toggle_fullscreen() {
        _fullscreen = !_fullscreen;
        SDL_SetWindowFullscreen(_p_window, _fullscreen);
    }
    auto size() -> vk::Extent2D {
        int width = 0;
        int height = 0;
        if (SDL_GetWindowSizeInPixels(_p_window, &width, &height)) fmt::println("{}", SDL_GetError());
        return vk::Extent2D((uint32_t)width, (uint32_t)height);
    }

    // window state
    SDL_Window* _p_window;
    bool _fullscreen = false;
    // vulkan
    vk::SurfaceKHR _surface;
    vk::DebugUtilsMessengerEXT _debug_msg;
    std::string _name = "Vulkan Renderer";
};