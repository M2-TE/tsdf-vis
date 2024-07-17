#pragma once
#include <vulkan/vulkan.hpp>
#include <VkBootstrap.h>
#include <vk_mem_alloc.hpp>
//
#include "window.hpp"

class Engine {
public:
    void init() {
        // Vulkan: dynamic dispatcher init 1/3
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
        
        // SDL: create window and query required instance extensions
        auto instance_extensions = _window.init(1280, 720);
        
        // VkBootstrap: create vulkan instance
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
        
        // SDL: create window and vulkan surface
        _window.create_surface(_instance, vkb_instance.debug_messenger);
        
        // VkBootstrap: select physical device
        vkb::PhysicalDeviceSelector selector(vkb_instance, _window._surface);
        std::vector<const char*> extensions { vk::KHRSwapchainExtensionName };
        vk::PhysicalDeviceFeatures vk_features { .fillModeNonSolid = true };
        vk::PhysicalDeviceVulkan11Features vk11_features {};
        vk::PhysicalDeviceVulkan12Features vk12_features { 
            .scalarBlockLayout = true,
            .timelineSemaphore = true,
            .bufferDeviceAddress = true,
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
    }
    void destroy() {
        _device.waitIdle();
        //
        // TODO: more
        _device.destroy();
        _window.destroy(_instance);
        _instance.destroy();
    }
    void execute_event(const SDL_Event* p_event) {
        
    }
    void execute_frame() {
        
    }
    
private:
    void rebuild() {}
    void handle_inputs() {}
    
public:
    bool _running;
    bool _rendering;
private:
    vk::Instance _instance;
    vk::PhysicalDevice _phys_device;
    vk::Device _device;
    vma::Allocator _vmalloc;
    Window _window;
};