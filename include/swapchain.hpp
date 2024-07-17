#pragma once
#include <vulkan/vulkan.hpp>
#include <VkBootstrap.h>
//
#include "window.hpp"
#include "queues.hpp"

class Swapchain {
    struct FrameData {
        void init(vk::Device device, Queues& queues) {
            vk::CommandPoolCreateInfo info_command_pool {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queues.i_graphics,
            };
            _command_pool = device.createCommandPool(info_command_pool);
            vk::CommandBufferAllocateInfo bufferInfo {
                .commandPool = _command_pool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            _command_buffer = device.allocateCommandBuffers(bufferInfo).front();

            vk::SemaphoreCreateInfo semaInfo {};
            _sema_swap_acquire = device.createSemaphore(semaInfo);
            _sema_swap_write = device.createSemaphore(semaInfo);
            vk::FenceCreateInfo fenceInfo { .flags = vk::FenceCreateFlagBits::eSignaled };
            _fence_present = device.createFence(fenceInfo);
        }
        void destroy(vk::Device device) {
            device.destroyCommandPool(_command_pool);
            device.destroySemaphore(_sema_swap_acquire);
            device.destroySemaphore(_sema_swap_write);
            device.destroyFence(_fence_present);
            }
        // command recording
        vk::CommandPool _command_pool;
        vk::CommandBuffer _command_buffer;
        // synchronization
        vk::Semaphore _sema_swap_acquire;
        vk::Semaphore _sema_swap_write;
        vk::Fence _fence_present;
    };
public:
    void init(vk::PhysicalDevice phys_device, vk::Device device, Window& window, Queues& queues) {
        // VkBoostrap: build swapchain
        vkb::SwapchainBuilder swapchain_builder(phys_device, device, window._surface);
        swapchain_builder.set_desired_extent(window.size().width, window.size().height)
            .set_desired_format(vk::SurfaceFormatKHR(vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear))
            .set_desired_present_mode((VkPresentModeKHR)vk::PresentModeKHR::eFifo)
            .add_image_usage_flags((VkImageUsageFlags)vk::ImageUsageFlagBits::eTransferDst);
        auto build = swapchain_builder.build();
        if (!build) fmt::println("VkBootstrap error: {}", build.error().message());
        vkb::Swapchain vkb_swapchain = build.value();
        
        // get swapchain components
        _extent = vkb_swapchain.extent;
        _format = vk::Format(vkb_swapchain.image_format);
        _swapchain = vkb_swapchain.swapchain;
        auto tmp_images = vkb_swapchain.get_images().value();
        auto tmp_views = vkb_swapchain.get_image_views().value();
        _images.resize(tmp_images.size());
        _image_views.resize(tmp_views.size());
        for (std::size_t i = 0; i < tmp_images.size(); i++) {
            _images[i] = tmp_images[i];
            _image_views[i] = tmp_views[i];
        }
        
        // Vulkan: create command pools and buffers
        _presentation_queue = queues.graphics;
        _frames.resize(vkb_swapchain.image_count);
        for (std::size_t i = 0; i < vkb_swapchain.image_count; i++) {
            _frames[i].init(device, queues);
        }
        _resize_requested = false;
    }
    void destroy(vk::Device device) {
        for (auto& frame: _frames) frame.destroy(device);
        for (auto& view: _image_views) device.destroyImageView(view);
        device.destroySwapchainKHR(_swapchain);
    }
    void resize(vk::PhysicalDevice physDevice, vk::Device device, Window& window, Queues& queues) { // TODO: proper resize
        destroy(device);
        init(physDevice, device, window, queues);
    }
    void present() {}
    
public:
    vk::SwapchainKHR _swapchain;
    std::vector<vk::Image> _images;
    std::vector<vk::ImageView> _image_views;
    vk::Extent2D _extent;
    vk::Format _format;
    vk::Queue _presentation_queue;
    bool _resize_requested = true;
private:
    std::vector<FrameData> _frames;
    uint32_t _sync_frame = 0;
};