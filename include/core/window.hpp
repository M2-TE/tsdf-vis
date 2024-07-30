#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>

struct Window {
    auto init(int width, int height, std::string name) -> vk::Instance {
        // SDL: init video subsystem for render surfaces
        if (SDL_InitSubSystem(SDL_INIT_VIDEO)) fmt::println("{}", SDL_GetError());
        
        // SDL: create window
        _window_p = SDL_CreateWindow(name.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (_window_p == nullptr) fmt::println("{}", SDL_GetError());

        // SDL: query required instance extensions
        uint32_t n_extensions;
        const char* const* p_extensions = SDL_Vulkan_GetInstanceExtensions(&n_extensions);
        if (p_extensions == nullptr) fmt::println("{}", SDL_GetError());
        // write to c++ vector
        std::vector<const char*> extensions(n_extensions);
        for (uint32_t i = 0; i < n_extensions; i++) extensions[i] = p_extensions[i];

        // optionally enable debug layers
        std::vector<const char*> layers;
#       ifdef VULKAN_VALIDATION_LAYERS
            layers.push_back("VK_LAYER_KHRONOS_validation");
#       endif

        // Vulkan: create instance
        vk::ApplicationInfo info_app {
            .pApplicationName = name.c_str(),
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .pEngineName = "TBD",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = vk::ApiVersion13
        };
        vk::InstanceCreateInfo info_instance {
            .pApplicationInfo = &info_app,
            .enabledLayerCount = (uint32_t)layers.size(),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = (uint32_t)extensions.size(),
            .ppEnabledExtensionNames = extensions.data()
        };
        vk::Instance instance = vk::createInstance(info_instance);

        // SDL: create surface
        VkSurfaceKHR surfaceTemp;
        if (SDL_Vulkan_CreateSurface(_window_p, instance, nullptr, &surfaceTemp)) fmt::println("{}", SDL_GetError());
        _surface = surfaceTemp;

        return instance;
    }
    void destroy(vk::Instance instance) {
        instance.destroySurfaceKHR(_surface);
        SDL_Quit();
    }
    
    void toggle_fullscreen() {
        _fullscreen = !_fullscreen;
        SDL_SetWindowFullscreen(_window_p, _fullscreen);
    }
    auto size() -> vk::Extent2D {
        int width = 0;
        int height = 0;
        if (SDL_GetWindowSizeInPixels(_window_p, &width, &height)) fmt::println("{}", SDL_GetError());
        return vk::Extent2D((uint32_t)width, (uint32_t)height);
    }

    SDL_Window* _window_p;
    vk::SurfaceKHR _surface;
    bool _fullscreen = false;
};