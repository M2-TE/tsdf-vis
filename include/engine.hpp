#pragma once
#include <vulkan/vulkan.hpp>
#include <VkBootstrap.h>
#include <vk_mem_alloc.hpp>
//
#include "core/device_selector.hpp"
#include "core/window.hpp"
#include "core/queues.hpp"
#include "core/swapchain.hpp"
#include "core/renderer.hpp"
#include "core/imgui.hpp"
#include "core/input.hpp"
#include "components/grid.hpp"

class Engine {
public:
    void init() {
        // Vulkan: dynamic dispatcher init 1/3
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        
        // SDL: create window and query required instance extensions
        auto instance_extensions = _window.init(1280, 720);
        
        // VkBootstrap: create vulkan instance // TODO: REPLACE VKB
        vkb::InstanceBuilder instance_builder;
        instance_builder.set_app_name(_window._name.c_str())
            .enable_extensions(instance_extensions)
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0);
        if (_window.using_debug_msg()) instance_builder.request_validation_layers();
        auto instance_build = instance_builder.build();
        if (!instance_build) fmt::println("VkBootstrap error: {}", instance_build.error().message());
        vkb::Instance& vkb_instance = instance_build.value();
        _instance = vkb_instance.instance;
        
        // Vulkan: dynamic dispatcher init 2/3
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);
        
        // SDL: create window and vulkan surface TODO: MOVE THIS AND INSTANCE CREATOR INTO WINDOW
        _window.create_surface(_instance, vkb_instance.debug_messenger);
        
        // Vulkan: select physical device
        DeviceSelector device_selector {
            ._required_major = 1,
            ._required_minor = 3,
            ._required_extensions = {
                vk::KHRSwapchainExtensionName,
            },
            ._required_features {
                .fillModeNonSolid = true,
            },
            ._required_vk11_features {},
            ._required_vk12_features {
                .timelineSemaphore = true,
            },
            ._required_vk13_features {
                .synchronization2 = true,
                .dynamicRendering = true,
            }
        };
        device_selector.select(_instance);
        exit(0);
        
        // VkBootstrap: select physical device
        vkb::PhysicalDeviceSelector selector(vkb_instance, _window._surface);
        std::vector<const char*> extensions { vk::KHRSwapchainExtensionName };
        vk::PhysicalDeviceFeatures vk_features { 
            .fillModeNonSolid = true
        };
        vk::PhysicalDeviceVulkan11Features vk11_features {
        };
        vk::PhysicalDeviceVulkan12Features vk12_features {
            .timelineSemaphore = true,
        };
        vk::PhysicalDeviceVulkan13Features vk13_features {
            .synchronization2 = true,
            .dynamicRendering = true,
        };
        selector.set_minimum_version(1, 3)
            .set_required_features(vk_features)
            .add_required_extensions(extensions)
            .set_required_features_11(vk11_features)
            .set_required_features_12(vk12_features)
            .set_required_features_13(vk13_features);
        auto device_selection = selector.select();
        if (!device_selection) fmt::println("VkBootstrap error: {}", device_selection.error().message());
        vkb::PhysicalDevice& vkb_phys_device = device_selection.value();
        _phys_device = vkb_phys_device.physical_device;
        
        // VkBootstrap: create device
        auto device_builder = vkb::DeviceBuilder(vkb_phys_device).build();
        if (!device_builder) fmt::println("VkBootstrap error: {}", device_builder.error().message());
        vkb::Device& vkb_device = device_builder.value();
        _device = vkb_device.device;
        
        // Vulkan: dynamic dispatcher init 3/3
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_device);
        
        // VMA: create allocator
        vma::VulkanFunctions vk_funcs {
            .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
        };
        vma::AllocatorCreateInfo info_vmalloc {
            .flags = 
                vma::AllocatorCreateFlagBits::eKhrDedicatedAllocation,
            .physicalDevice = _phys_device,
            .device = _device,
            .pVulkanFunctions = &vk_funcs,
            .instance = _instance,
            .vulkanApiVersion = vk::ApiVersion13,
        };
        _vmalloc = vma::createAllocator(info_vmalloc);
        
        // create renderer
        _queues.init(_device, vkb_device);
        _camera.init(_vmalloc, _queues.i_graphics, _window.size());
        _swapchain._resize_requested = true;
        
        // initialize imgui backend
        ImGui::impl::init_sdl(_window._p_window);
        ImGui::impl::init_vulkan(_instance, _device, _phys_device, _queues, vk::Format::eR16G16B16A16Sfloat);
        
        _running = true;
        _rendering = true;
        
        gridtests();
    }
    void destroy() {
        _device.waitIdle();
        //
        ImGui::impl::shutdown(_device);
        _grid.destroy(_vmalloc);
        _camera.destroy(_vmalloc);
        _renderer.destroy(_device, _vmalloc);
        _swapchain.destroy(_device);
        _vmalloc.destroy();
        _device.destroy();
        _window.destroy(_instance);
        _instance.destroy();
    }
    
    void execute_event(const SDL_Event* p_event) {
        ImGui::impl::process_event(p_event);
        switch (p_event->type) {
            // window handling
            case SDL_EventType::SDL_EVENT_QUIT: _running = false; break;
            case SDL_EventType::SDL_EVENT_WINDOW_RESTORED: _rendering = true; break;
            case SDL_EventType::SDL_EVENT_WINDOW_MINIMIZED: _rendering = false; break;
            case SDL_EventType::SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: _swapchain._resize_requested = true; break;
            // input handling
            case SDL_EventType::SDL_EVENT_KEY_UP: Input::register_key_up(p_event->key); break;
            case SDL_EventType::SDL_EVENT_KEY_DOWN: Input::register_key_down(p_event->key); break;
            case SDL_EventType::SDL_EVENT_MOUSE_MOTION: Input::register_motion(p_event->motion);break;
            case SDL_EventType::SDL_EVENT_MOUSE_BUTTON_UP: Input::register_button_up(p_event->button); break;
            case SDL_EventType::SDL_EVENT_MOUSE_BUTTON_DOWN: Input::register_button_down(p_event->button); break;
            case SDL_EventType::SDL_EVENT_WINDOW_FOCUS_LOST: Input::flush_all(); break;
            default: break;
        }
    }
    void execute_frame() {
        if (_swapchain._resize_requested) rebuild();
        handle_inputs();
        ImGui::impl::new_frame();
        ImGui::utils::display_fps();
        _camera.update(_vmalloc);
        _renderer.render(_device, _swapchain, _queues, _grid);
        Input::flush();
    }
    
private:
    void gridtests() {
        _grid.init(_vmalloc, _queues.i_graphics);
    }
    void rebuild() {
        _device.waitIdle();
        SDL_SyncWindow(_window._p_window);
        
        fmt::println("renderer rebuild triggered");
        _camera.resize(_window.size());
        _renderer.resize(_device, _vmalloc, _queues, _window.size(), _camera, _grid);
        _swapchain.resize(_phys_device, _device, _window, _queues);
    }
    void handle_inputs() {
        // fullscreen toggle via F11
        if (Keys::pressed(SDLK_F11)) _window.toggle_fullscreen();
        // handle mouse capture
        if (!Keys::down(SDLK_LALT) && Mouse::pressed(Mouse::ids::left) && !SDL_GetRelativeMouseMode()) SDL_SetRelativeMouseMode(true);
        if (Keys::pressed(SDLK_LALT) && SDL_GetRelativeMouseMode()) SDL_SetRelativeMouseMode(false);
        if (Keys::released(SDLK_LALT) && !SDL_GetRelativeMouseMode()) SDL_SetRelativeMouseMode(true);
        if (Keys::pressed(SDLK_ESCAPE) && SDL_GetRelativeMouseMode()) SDL_SetRelativeMouseMode(false);
    }
    
public:
    bool _running;
    bool _rendering;
private:
    vk::Instance _instance;
    vk::PhysicalDevice _phys_device;
    vk::Device _device;
    vma::Allocator _vmalloc;
    Window _window;
    Queues _queues;
    Swapchain _swapchain;
    Renderer _renderer;
    Camera _camera;
    Grid _grid;
};