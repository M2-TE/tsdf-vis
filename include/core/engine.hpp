#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <SDL3/SDL_events.h>
#include "core/device_selector.hpp"
#include "core/window.hpp"
#include "core/queues.hpp"
#include "core/swapchain.hpp"
#include "core/renderer.hpp"
#include "core/imgui.hpp"
#include "core/input.hpp"
#include "components/scene.hpp"

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
            ._preferred_device_type = vk::PhysicalDeviceType::eDiscreteGpu,
            ._required_extensions {
                vk::KHRSwapchainExtensionName,
                // vk::KHRDynamicRenderingLocalReadExtensionName,
            },
            ._optional_extensions {
                vk::EXTMemoryPriorityExtensionName,
                vk::EXTPageableDeviceLocalMemoryExtensionName,
            },
            ._required_features {
                .fillModeNonSolid = true,
                .wideLines = true,
            },
            ._required_vk11_features {
            },
            ._required_vk12_features {
                // .bufferDeviceAddress = true,
            },
            ._required_vk13_features {
                .synchronization2 = true,
                .dynamicRendering = true,
                .maintenance4 = true,
            },
            ._required_queues {
                vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer,
                vk::QueueFlagBits::eGraphics,
                vk::QueueFlagBits::eCompute,
                vk::QueueFlagBits::eTransfer,
            }
        };
        _phys_device = device_selector.select_physical_device(_instance, _window._surface);

        // Vulkan: create device
        std::vector<uint32_t> queue_mappings;
        std::tie(_device, queue_mappings) = device_selector.create_logical_device(_phys_device);
        
        // Vulkan: dynamic dispatcher init 3/3
        VULKAN_HPP_DEFAULT_DISPATCHER.init(_device);
        
        // VMA: create allocator
        vma::VulkanFunctions vk_funcs {
            .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
        };
        vma::AllocatorCreateInfo info_vmalloc {
            .flags = vma::AllocatorCreateFlagBits::eKhrDedicatedAllocation,
            .physicalDevice = _phys_device,
            .device = _device,
            .pVulkanFunctions = &vk_funcs,
            .instance = _instance,
            .vulkanApiVersion = vk::ApiVersion13,
        };
        _vmalloc = vma::createAllocator(info_vmalloc);

        // create renderer components
        DepthStencil::set_format(_phys_device);
        _queues.init(_device, queue_mappings);
        _swapchain.set_target_framerate(_fps_foreground);
        _swapchain._resize_requested = true;
        
        // initialize imgui backend
        ImGui::impl::init_sdl(_window._window_p);
        ImGui::impl::init_vulkan(_instance, _device, _phys_device, _queues._universal, vk::Format::eR16G16B16A16Sfloat);
        _rendering = true;
        
        // begin constructing scenes
        _scene.init(_vmalloc, _queues._universal_i);
    }
    void destroy() {
        _device.waitIdle();
        // destroy scenes
        _scene.destroy(_vmalloc);
        //
        ImGui::impl::shutdown(_device);
        _renderer.destroy(_device, _vmalloc);
        _swapchain.destroy(_device);
        _queues.destroy(_device);
        _vmalloc.destroy();
        _device.destroy();
        _window.destroy(_instance);
        _instance.destroy();
    }
    
    auto execute_event(const SDL_Event* event_p) -> SDL_AppResult {
        ImGui::impl::process_event(event_p);
        switch (event_p->type) {
            // window handling
            case SDL_EventType::SDL_EVENT_QUIT: return SDL_AppResult::SDL_APP_SUCCESS;
            case SDL_EventType::SDL_EVENT_WINDOW_RESTORED: _rendering = true; break;
            case SDL_EventType::SDL_EVENT_WINDOW_MINIMIZED: _rendering = false; break;
            case SDL_EventType::SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                fmt::println("Window resized to {}x{}", event_p->window.data1, event_p->window.data2);
                _swapchain._resize_requested = true; 
                break;
            }
            // input handling
            case SDL_EventType::SDL_EVENT_KEY_UP: Input::register_key_up(event_p->key); break;
            case SDL_EventType::SDL_EVENT_KEY_DOWN: Input::register_key_down(event_p->key); break;
            case SDL_EventType::SDL_EVENT_MOUSE_MOTION: Input::register_motion(event_p->motion);break;
            case SDL_EventType::SDL_EVENT_MOUSE_BUTTON_UP: Input::register_button_up(event_p->button); break;
            case SDL_EventType::SDL_EVENT_MOUSE_BUTTON_DOWN: Input::register_button_down(event_p->button); break;
            case SDL_EventType::SDL_EVENT_WINDOW_FOCUS_LOST: {
                _swapchain.set_target_framerate(_fps_background);
                Input::flush_all();
                break;
            }
            case SDL_EventType::SDL_EVENT_WINDOW_FOCUS_GAINED: {
                _swapchain.set_target_framerate(_fps_foreground);
                Input::flush_all();
                break;
            }
            default: break;
        }
        return SDL_AppResult::SDL_APP_CONTINUE;
    }
    void execute_frame() {
        if (!_rendering) {
            SDL_Delay(50);
            return;
        }
        if (_swapchain._resize_requested) {
            resize();
            return;
        }
        handle_inputs();
        ImGui::impl::new_frame();
        ImGui::utils::display_fps();

        _scene.update_safe();
        _renderer.wait(_device);
        _scene.update(_vmalloc);
        _renderer.render(_device, _swapchain, _queues, _scene);
        Input::flush();
    }
    
private:
    void resize() {
        _device.waitIdle();
        if (!SDL_SyncWindow(_window._window_p)) {
            fmt::println("Failed to sync window");
            return;
        }
        
        _scene._camera.resize(_window.size());
        _renderer.resize(_device, _vmalloc, _queues, _window.size(), _scene._camera);
        _swapchain.resize(_phys_device, _device, _window, _queues);
    }
    void handle_inputs() {
        // fullscreen toggle via F11
        if (Keys::pressed(SDLK_F11)) {
            _window.toggle_fullscreen();
            _swapchain._resize_requested = true;
        }
        // handle mouse capture
        if (!Keys::down(SDLK_LALT) && Mouse::pressed(Mouse::ids::left) && !Mouse::captured()) SDL_SetWindowRelativeMouseMode(_window._window_p, true);
        if (Keys::pressed(SDLK_LALT) && Mouse::captured()) SDL_SetWindowRelativeMouseMode(_window._window_p, false);
        if (Keys::released(SDLK_LALT) && !Mouse::captured()) SDL_SetWindowRelativeMouseMode(_window._window_p, true);
        if (Keys::pressed(SDLK_ESCAPE) && Mouse::captured()) SDL_SetWindowRelativeMouseMode(_window._window_p, false);
        Input::register_capture(SDL_GetWindowRelativeMouseMode(_window._window_p));
    }
    
private:
    vk::Instance _instance;
    vk::PhysicalDevice _phys_device;
    vk::Device _device;
    vma::Allocator _vmalloc;
    Window _window;
    Queues _queues;
    Swapchain _swapchain;
    Renderer _renderer;
    Scene _scene;
    uint32_t _fps_foreground = 0;
    uint32_t _fps_background = 5;
    bool _rendering;
};