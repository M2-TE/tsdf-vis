#pragma once
#include <vulkan/vulkan.hpp>
#include <VkBootstrap.h>

struct Queues {
    void init(vk::Device device, vkb::Device& vkb_device) {
        // graphics
        graphics = vkb_device.get_queue(vkb::QueueType::graphics).value();
        i_graphics = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
        // compute (dedicated)
        auto vkb_compute = vkb_device.get_dedicated_queue(vkb::QueueType::compute);
        compute = graphics;
        if (vkb_compute) compute = vkb_compute.value();
        auto i_vkb_compute = vkb_device.get_dedicated_queue_index(vkb::QueueType::compute);
        i_compute = i_graphics;
        if (i_vkb_compute) i_compute = vkb_device.get_dedicated_queue_index(vkb::QueueType::compute).value();
        // transfer (dedicated)
        auto vkb_transfer = vkb_device.get_dedicated_queue(vkb::QueueType::transfer);
        transfer = graphics;
        if (vkb_transfer) transfer = vkb_transfer.value();
        auto i_vkb_transfer = vkb_device.get_dedicated_queue_index(vkb::QueueType::transfer);
        i_transfer = i_graphics;
        if (i_vkb_transfer) i_transfer = vkb_device.get_dedicated_queue_index(vkb::QueueType::transfer).value();
    }
    
    // queue handles
    vk::Queue graphics, compute, transfer;
    // queue indices
    uint32_t i_graphics, i_compute, i_transfer;
};