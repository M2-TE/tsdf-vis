#pragma once
#include <vulkan/vulkan.hpp>
#include <VkBootstrap.h>
//
#include "window.hpp"
#include "queues.hpp"
#include "image.hpp"
#include "imgui.hpp"

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
        fmt::println("resized to: {}x{}", window.size().width, window.size().height);
    }
    void present(vk::Device device, Image& src_image, vk::Semaphore timeline, uint64_t& timeline_val) {
        // wait for this frame's fence to be signaled and reset it
        FrameData& frame = _frames[_sync_frame++ % _frames.size()];
        while (vk::Result::eTimeout == device.waitForFences(frame._fence_present, vk::True, UINT64_MAX));
        device.resetFences(frame._fence_present);
        
        // acquire image from swapchain
        uint32_t swap_index;
        for (vk::Result result = vk::Result::eTimeout; result == vk::Result::eTimeout;) {
            std::tie(result, swap_index) = device.acquireNextImageKHR(_swapchain, UINT64_MAX, frame._sema_swap_acquire);
        }
        
        
        // restart command buffer
        vk::CommandBuffer cmd = frame._command_buffer;
        vk::CommandBufferBeginInfo info_cmd_begin {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        };
        cmd.begin(info_cmd_begin);

        // draw ImGui UI onto input image
        Image::TransitionInfo info_transition {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eAllCommands,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .src_access = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .dst_access = vk::AccessFlagBits2::eMemoryWrite,
        };
        src_image.transition_layout(info_transition);
        ImGui::impl::draw(cmd, src_image._view, info_transition.new_layout, _extent);
        
        
        // transition source image layout for upcoming blit
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eTransferSrcOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_stage = vk::PipelineStageFlagBits2::eBlit,
            .src_access = vk::AccessFlagBits2::eMemoryWrite,
            .dst_access = vk::AccessFlagBits2::eMemoryRead,
        };
        src_image.transition_layout(info_transition);
        
        // transition swapchain image layout for upcoming blit
        vk::ImageMemoryBarrier2 image_barrier {
            .srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryRead,
            .dstStageMask = vk::PipelineStageFlagBits2::eBlit,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .image = _images[swap_index],
            .subresourceRange { 
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        vk::DependencyInfo info_dep {
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &image_barrier,  
        };
        cmd.pipelineBarrier2(info_dep);
        
        // perform blit from source to swapchain image
        vk::ImageBlit2 region {
            .srcSubresource { 
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets = std::array<vk::Offset3D, 2>{ 
                vk::Offset3D(), 
                vk::Offset3D(src_image._extent.width, src_image._extent.height, 1) },
            .dstSubresource { 
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffsets = std::array<vk::Offset3D, 2>{ 
                vk::Offset3D(), 
                vk::Offset3D(_extent.width, _extent.height, 1) },
        };
        vk::BlitImageInfo2 info_blit {
            .srcImage = src_image._image,
            .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
            .dstImage = _images[swap_index],
            .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
            .regionCount = 1,
            .pRegions = &region,
            .filter = vk::Filter::eLinear,
        };
        cmd.blitImage2(info_blit);
        
        // transition swapchain image into presentation layout
        image_barrier = {
            .srcStageMask = vk::PipelineStageFlagBits2::eBlit,
            .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
            .dstAccessMask = vk::AccessFlagBits2::eMemoryRead,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .image = _images[swap_index],
            .subresourceRange {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        cmd.pipelineBarrier2(info_dep);
        cmd.end();
        
        // submit command buffer to graphics queue
        std::array<vk::SemaphoreSubmitInfo, 2> infos_wait {
            vk::SemaphoreSubmitInfo {
                .semaphore = timeline,
                .value = timeline_val++,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
            },
            vk::SemaphoreSubmitInfo {
                .semaphore = frame._sema_swap_acquire,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands,  
            },
        };
        std::array<vk::SemaphoreSubmitInfo, 2> infos_signal {
            vk::SemaphoreSubmitInfo {
                .semaphore = timeline,
                .value = timeline_val,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
            },
            vk::SemaphoreSubmitInfo {
                .semaphore = frame._sema_swap_write,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands,  
            },
        };
        vk::CommandBufferSubmitInfo info_cmd_submit { .commandBuffer = cmd };
        vk::SubmitInfo2 info_submit {
            .waitSemaphoreInfoCount = (uint32_t)infos_wait.size(),
            .pWaitSemaphoreInfos = infos_wait.data(),
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &info_cmd_submit,
            .signalSemaphoreInfoCount = (uint32_t)infos_signal.size(),
            .pSignalSemaphoreInfos = infos_signal.data(),  
        };
        _presentation_queue.submit2(info_submit, frame._fence_present);

        // present swapchain image
        vk::PresentInfoKHR presentInfo {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame._sema_swap_write,
            .swapchainCount = 1,
            .pSwapchains = &_swapchain,
            .pImageIndices = &swap_index  
        };
        
        vk::Result result = _presentation_queue.presentKHR(presentInfo);
        if (result == vk::Result::eErrorOutOfDateKHR) _resize_requested = true;
    }
    
public:
    vk::SwapchainKHR _swapchain;
    std::vector<vk::Image> _images;
    std::vector<vk::ImageView> _image_views;
    vk::Extent2D _extent;
    vk::Format _format;
    vk::Queue _presentation_queue;
    bool _resize_requested;
private:
    std::vector<FrameData> _frames;
    uint32_t _sync_frame = 0;
};