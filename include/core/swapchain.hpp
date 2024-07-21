#pragma once
#include <vulkan/vulkan.hpp>
#include <VkBootstrap.h>
//
#include "core/window.hpp"
#include "core/queues.hpp"
#include "core/imgui.hpp"
#include "components/image.hpp"

class Swapchain {
    struct FrameData {
        void init(vk::Device device, Queues& queues) {
            vk::CommandPoolCreateInfo info_command_pool {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queues._family_universal,
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
        for (std::size_t i = 0; i < tmp_images.size(); i++) {
            Image::WrapInfo info_wrap {
                .image = tmp_images[i],
                .image_view = tmp_views[i],
                .extent = { _extent.width, _extent.height, 0 },
                .aspects = vk::ImageAspectFlagBits::eColor
            };
            _images[i].wrap(info_wrap);
        }
        
        // Vulkan: create command pools and buffers
        _presentation_queue = queues._universal;
        _frames.resize(vkb_swapchain.image_count);
        for (std::size_t i = 0; i < vkb_swapchain.image_count; i++) {
            _frames[i].init(device, queues);
        }
        _resize_requested = false;
    }
    void destroy(vk::Device device) {
        for (auto& frame: _frames) frame.destroy(device);
        for (auto& image: _images) device.destroyImageView(image._view);
        if (_images.size() > 0) device.destroySwapchainKHR(_swapchain);
        _frames.clear();
        _images.clear();
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
        draw_imgui(cmd, src_image);
        draw_swapchain(cmd, src_image, swap_index);
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

private:
    void draw_imgui(vk::CommandBuffer cmd, Image& dst_image) {
        Image::TransitionInfo info_transition {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eAllCommands,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .src_access = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
            .dst_access = vk::AccessFlagBits2::eMemoryWrite,
        };
        dst_image.transition_layout(info_transition);
        ImGui::impl::draw(cmd, dst_image._view, info_transition.new_layout, _extent);
    }
    void draw_swapchain(vk::CommandBuffer cmd, Image& src_image, uint32_t swap_index) {
        // transition source image layout for upcoming blit
        Image::TransitionInfo info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eTransferSrcOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_stage = vk::PipelineStageFlagBits2::eBlit,
            .src_access = vk::AccessFlagBits2::eMemoryWrite,
            .dst_access = vk::AccessFlagBits2::eMemoryRead,
        };
        src_image.transition_layout(info_transition);
        // transition swapchain image layout for upcoming blit
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eTransferDstOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eAllCommands,
            .dst_stage = vk::PipelineStageFlagBits2::eBlit,
            .src_access = vk::AccessFlagBits2::eMemoryRead,
            .dst_access = vk::AccessFlagBits2::eMemoryWrite,
        };
        _images[swap_index].transition_layout(info_transition);
        // perform blit from source to swapchain image
        _images[swap_index].blit(cmd, src_image);

        // transition swapchain image into presentation layout
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::ePresentSrcKHR,
            .src_stage = vk::PipelineStageFlagBits2::eBlit,
            .dst_stage = vk::PipelineStageFlagBits2::eAllCommands,
            .src_access = vk::AccessFlagBits2::eMemoryWrite,
            .dst_access = vk::AccessFlagBits2::eMemoryRead,
        };
        _images[swap_index].transition_layout(info_transition);
    }

public:
    vk::SwapchainKHR _swapchain;
    std::vector<Image> _images;
    vk::Extent2D _extent;
    vk::Format _format;
    vk::Queue _presentation_queue;
    bool _resize_requested;
private:
    std::vector<FrameData> _frames;
    uint32_t _sync_frame = 0;
};