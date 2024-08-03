#pragma once
#include <vulkan/vulkan.hpp>

struct Queues {
    void init(vk::Device device, std::span<uint32_t> queue_mappings) {
        _universal_i = queue_mappings[0];
        _graphics_i = queue_mappings[1];
        _compute_i = queue_mappings[2];
        _transfer_i = queue_mappings[3];
        _universal = device.getQueue(_universal_i, 0);
        _graphics = device.getQueue(_graphics_i, 0);
        _compute = device.getQueue(_compute_i, 0);
        _transfer = device.getQueue(_transfer_i, 0);

        vk::CommandPoolCreateInfo info {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = _universal_i,
        };
    }
    // TODO: allow oneshot command buffer on queues
    
    // queue handle
    vk::Queue _universal, _graphics, _compute, _transfer;
    // queue family index
    uint32_t _universal_i, _graphics_i, _compute_i, _transfer_i;
private:
    // oneshot command buffers and pools
    vk::CommandPool _universal_pool, _graphics_pool, _compute_pool, _transfer_pool;
    vk::CommandBuffer _universal_cmd, _graphics_cmd, _compute_cmd, _transfer_cmd;
};