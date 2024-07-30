#pragma once
#include <vulkan/vulkan.hpp>

struct Queues {
    void init(vk::Device device, std::span<uint32_t> queue_mappings) {
        _universal_i = queue_mappings[0];
        _graphics_i = queue_mappings[1];
        compute_i = queue_mappings[2];
        transfer_i = queue_mappings[3];
        _universal = device.getQueue(_universal_i, 0);
        _graphics = device.getQueue(_graphics_i, 0);
        _compute = device.getQueue(compute_i, 0);
        _transfer = device.getQueue(transfer_i, 0);
    }
    
    // queue handle
    vk::Queue _universal, _graphics, _compute, _transfer;
    // queue family index
    uint32_t _universal_i, _graphics_i, compute_i, transfer_i;
};