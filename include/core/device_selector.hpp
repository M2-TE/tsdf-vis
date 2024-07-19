#pragma once
#include <utility>
//
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>

struct DeviceSelector {
    auto init(vk::Instance instance) -> std::pair<vk::PhysicalDevice, vk::Device> {
        auto phys_devices = instance.enumeratePhysicalDevices();
        
        std::vector<vk::PhysicalDevice> discrete_devices;
        fmt::println("Available devices:");
        for (vk::PhysicalDevice device: phys_devices) {
            vk::PhysicalDeviceProperties props = device.getProperties();
            fmt::println("\tID {}: {}", props.deviceID, (const char*)props.deviceName);
            
            // check if its a discrete gpu
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                discrete_devices.push_back(device);
            }
        }
        
        vk::PhysicalDevice phys_device = discrete_devices.front();
        vk::PhysicalDeviceProperties props = phys_device.getProperties();
        
        
        vk::Device device;
        return std::make_pair(phys_device, device);
    }
    void destroy() {
        
    }
};