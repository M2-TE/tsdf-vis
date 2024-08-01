#pragma once
#include <set>
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
        uint32_t extensions_n;
        const char* const* extensions_p = SDL_Vulkan_GetInstanceExtensions(&extensions_n);
        if (extensions_p == nullptr) fmt::println("{}", SDL_GetError());
        // write to c++ vector
        std::vector<const char*> extensions(extensions_n);
        for (uint32_t i = 0; i < extensions_n; i++) extensions[i] = extensions_p[i];

        // check availability of extensions requested by sdl
        auto available_extensions = vk::enumerateInstanceExtensionProperties();
        std::set<std::string> extension_set;
        std::copy(extensions.cbegin(), extensions.cend(), std::inserter(extension_set, extension_set.end()));
        for (auto& ext: available_extensions) {
            std::string ext_str = ext.extensionName;
            auto ext_it = extension_set.find(ext_str);
            if (ext_it != extension_set.end()) extension_set.erase(ext_it);
        }
        if (extension_set.size() > 0) {
            fmt::println("Missing vital instance extensions required by SDL:");
            for (auto& ext: extension_set) fmt::println("\t{}", ext);
            exit(0);
        }

        // optionally enable debug layers
        std::vector<const char*> layers;
#       ifdef VULKAN_VALIDATION_LAYERS
            std::string validation_layer = "VK_LAYER_KHRONOS_validation";
            // only request if validation layers are available
            auto layer_props = vk::enumerateInstanceLayerProperties();
            bool available = false;
            for (auto& layer: layer_props) {
                auto res = std::strcmp(layer.layerName, validation_layer.data());
                if (res) {
                    available = true;
                    break;
                }
            }
            if (available) {
                layers.push_back(validation_layer.data());
                fmt::println("success");
            }
            else fmt::println("Validation layers requested but not present");
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