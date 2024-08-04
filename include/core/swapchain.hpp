#pragma once
#include <vulkan/vulkan.hpp>
#include "core/window.hpp"
#include "core/queues.hpp"
#include "core/imgui.hpp"
#include "components/image.hpp"

class Swapchain {
    struct SyncFrame {
        void init(vk::Device device, Queues& queues) {
            vk::CommandPoolCreateInfo info_command_pool {
                .queueFamilyIndex = queues._universal_i,
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
        // query swapchain properties
        vk::SurfaceCapabilitiesKHR capabilities = phys_device.getSurfaceCapabilitiesKHR(window._surface);
        std::vector<vk::SurfaceFormatKHR> formats = phys_device.getSurfaceFormatsKHR(window._surface);
        _extent = window.size();
        _presentation_queue = queues._universal;
        
        // pick color space and format
        vk::ColorSpaceKHR color_space = formats.front().colorSpace;
        _format = formats.front().format;
        for (auto format: formats) {
            bool format_requirement = 
                format.format == vk::Format::eR8G8B8A8Srgb ||
                format.format == vk::Format::eB8G8R8A8Srgb ||
                format.format == vk::Format::eR8G8B8Srgb ||
                format.format == vk::Format::eB8G8R8Srgb;
            bool color_requirement = 
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            if (format_requirement && color_requirement) {
                _format = format.format;
                color_space = format.colorSpace;
                break;
            }
        }

        uint32_t swapchain_image_count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && swapchain_image_count > capabilities.maxImageCount) {
            swapchain_image_count = capabilities.maxImageCount;
        }

        // create swapchain
        vk::SwapchainCreateInfoKHR info_swapchain {
            .surface = window._surface,
            .minImageCount = swapchain_image_count,
            .imageFormat = _format,
            .imageColorSpace = color_space,
            .imageExtent = window.size(),
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queues._universal_i,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = vk::PresentModeKHR::eFifo,
            .clipped = true,
            .oldSwapchain = nullptr,
        };
        _swapchain = device.createSwapchainKHR(info_swapchain);

        // retrieve and wrap swapchain images
        std::vector<vk::Image> images = device.getSwapchainImagesKHR(_swapchain);
        for (vk::Image image: images) {
            Image::WrapInfo info_wrap {
                .image = image,
                .extent = { _extent.width, _extent.height, 0 },
                .aspects = vk::ImageAspectFlagBits::eColor
            };
            _images.emplace_back().wrap(info_wrap);
        }

        // create command pools and buffers
        _sync_frames.resize(images.size());
        for (std::size_t i = 0; i < images.size(); i++) {
            _sync_frames[i].init(device, queues);
        }
        _resize_requested = false;
    }
    void destroy(vk::Device device) {
        for (auto& frame: _sync_frames) frame.destroy(device);
        for (auto& image: _images) device.destroyImageView(image._view);
        if (_images.size() > 0) device.destroySwapchainKHR(_swapchain);
        _sync_frames.clear();
        _images.clear();
    }
    
    void resize(vk::PhysicalDevice physDevice, vk::Device device, Window& window, Queues& queues) {
        destroy(device);
        init(physDevice, device, window, queues);
        fmt::println("resized to: {}x{}", window.size().width, window.size().height);
    }
    void present(vk::Device device, Image& src_image, vk::Semaphore timeline, uint64_t& timeline_val) {
        // wait for this frame's fence to be signaled and reset it
        SyncFrame& frame = _sync_frames[_sync_frame_i++ % _sync_frames.size()];
        while (vk::Result::eTimeout == device.waitForFences(frame._fence_present, vk::True, UINT64_MAX));
        device.resetFences(frame._fence_present);

        // acquire image from swapchain
        uint32_t swap_index;
        for (auto result = vk::Result::eTimeout; result == vk::Result::eTimeout;) {
            std::tie(result, swap_index) = device.acquireNextImageKHR(_swapchain, UINT64_MAX, frame._sema_swap_acquire);
        }
        
        // restart command buffer
        device.resetCommandPool(frame._command_pool);
        vk::CommandBuffer cmd = frame._command_buffer;
        cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        // draw_imgui(cmd, src_image);
        draw_swapchain(cmd, src_image, swap_index);
        // transition swapchain image into presentation layout
        Image::TransitionInfo info_transition {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::ePresentSrcKHR,
            .dst_stage = vk::PipelineStageFlagBits2::eBottomOfPipe,
            .dst_access = vk::AccessFlagBits2::eNone,
        };
        _images[swap_index].transition_layout(info_transition);
        cmd.end();
        
        // submit command buffer to graphics queue
        std::array<vk::SemaphoreSubmitInfo, 2> infos_wait {
            vk::SemaphoreSubmitInfo {
                .semaphore = timeline,
                .value = timeline_val++,
                .stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
            },
            vk::SemaphoreSubmitInfo {
                .semaphore = frame._sema_swap_acquire,
                .stageMask = vk::PipelineStageFlagBits2::eTopOfPipe,  
            },
        };
        std::array<vk::SemaphoreSubmitInfo, 2> infos_signal {
            vk::SemaphoreSubmitInfo {
                .semaphore = timeline,
                .value = timeline_val,
                .stageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
            },
            vk::SemaphoreSubmitInfo {
                .semaphore = frame._sema_swap_write,
                .stageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,  
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
            .new_layout = vk::ImageLayout::eColorAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
        };
        dst_image.transition_layout(info_transition);
        ImGui::impl::draw(cmd, dst_image._view, info_transition.new_layout, _extent);
    }
    void draw_swapchain(vk::CommandBuffer cmd, Image& src_image, uint32_t swap_index) {
        // perform blit from source to swapchain image
        Image::TransitionInfo info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eTransferSrcOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eBlit,
            .dst_access = vk::AccessFlagBits2::eTransferRead,
        };
        src_image.transition_layout(info_transition);
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eTransferDstOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eBlit,
            .dst_access = vk::AccessFlagBits2::eTransferWrite,
        };
        _images[swap_index].transition_layout(info_transition);
        _images[swap_index].blit(cmd, src_image);
    }

public:
    vk::SwapchainKHR _swapchain;
    std::vector<Image> _images;
    vk::Extent2D _extent;
    vk::Format _format;
    vk::Queue _presentation_queue;
    bool _resize_requested;
private:
    std::vector<SyncFrame> _sync_frames;
    uint32_t _sync_frame_i = 0;
};