#pragma once
#include <vulkan/vulkan.hpp>

struct Queues {
    void init(vk::Device device, std::span<uint32_t> queue_mappings) {
        _family_universal = queue_mappings[0];
        _family_graphics = queue_mappings[1];
        _family_compute = queue_mappings[2];
        _family_transfer = queue_mappings[3];
        _universal = device.getQueue(_family_universal, 0);
        _graphics = device.getQueue(_family_graphics, 0);
        _compute = device.getQueue(_family_compute, 0);
        _transfer = device.getQueue(_family_transfer, 0);
    }
    
    // queue handle
    vk::Queue _universal, _graphics, _compute, _transfer;
    // queue family index
    uint32_t _family_universal, _family_graphics, _family_compute, _family_transfer;
};