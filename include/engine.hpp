#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <SDL3/SDL_events.h>
//
#include "utils/device_selector.hpp"
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
        _instance = _window.init(1280, 720, "TSDF Visualizer");
        
        // Vulkan: dynamic dispatcher init 2/3
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);
        
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
            },
            ._required_queues {
                vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer,
                vk::QueueFlagBits::eGraphics,
                vk::QueueFlagBits::eCompute,
                vk::QueueFlagBits::eTransfer,
            }
        };
        _phys_device = device_selector.select_physical_device(_instance);


        // Vulkan: create device
        std::vector<uint32_t> queue_mappings;
        std::tie(_device, queue_mappings) = device_selector.create_logical_device(_phys_device);
        _queues.init(_device, queue_mappings);
        
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
        
        // create renderer components
        _camera.init(_vmalloc, _queues._family_universal, _window.size());
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
        if (_swapchain._resize_requested) resize();
        ImGui::impl::new_frame();
        ImGui::utils::display_fps();

        handle_inputs();
        _camera.update(_vmalloc);
        _renderer.render(_device, _swapchain, _queues, _grid);
        Input::flush();
    }
    
private:
    void gridtests() {
        _grid.init(_vmalloc, _queues._family_universal);
    }
    void resize() {
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