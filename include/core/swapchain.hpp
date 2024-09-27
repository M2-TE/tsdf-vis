#pragma once
#include <chrono>
#include <thread>
#include <vulkan/vulkan.hpp>
#include "core/window.hpp"
#include "core/queues.hpp"
#include "core/imgui.hpp"
#include "core//image.hpp"

class Swapchain {
    struct SyncFrame {
        void init(vk::Device device, Queues& queues) {
            _command_pool = device.createCommandPool({ .queueFamilyIndex = queues._graphics_i });
            vk::CommandBufferAllocateInfo bufferInfo {
                .commandPool = _command_pool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            _command_buffer = device.allocateCommandBuffers(bufferInfo).front();

            vk::SemaphoreCreateInfo semaInfo {};
            _ready_to_record = device.createFence({ .flags = vk::FenceCreateFlagBits::eSignaled });
            _ready_to_write = device.createSemaphore(semaInfo);
            _ready_to_read = device.createSemaphore(semaInfo);
        }
        void destroy(vk::Device device) {
            device.destroyCommandPool(_command_pool);
            device.destroyFence(_ready_to_record);
            device.destroySemaphore(_ready_to_write);
            device.destroySemaphore(_ready_to_read);
        }
        // command recording
        vk::CommandPool _command_pool;
        vk::CommandBuffer _command_buffer;
        // synchronization
        vk::Fence _ready_to_record;
        vk::Semaphore _ready_to_write;
        vk::Semaphore _ready_to_read;
    };
public:
    void init(vk::PhysicalDevice phys_device, vk::Device device, Window& window, Queues& queues) {
        // query swapchain properties
        vk::SurfaceCapabilitiesKHR capabilities = phys_device.getSurfaceCapabilitiesKHR(window._surface);
        std::vector<vk::SurfaceFormatKHR> formats = phys_device.getSurfaceFormatsKHR(window._surface);
        _presentation_queue = queues._universal;
        _extent = window.size();
        // adjust extent in case surface has changed, but no window resize event was received yet
        if (_extent.width < capabilities.minImageExtent.width) _extent.width = capabilities.minImageExtent.width;
        if (_extent.width > capabilities.maxImageExtent.width) _extent.width = capabilities.maxImageExtent.width;
        if (_extent.height < capabilities.minImageExtent.height) _extent.height = capabilities.minImageExtent.height;
        if (_extent.height > capabilities.maxImageExtent.height) _extent.height = capabilities.maxImageExtent.height;
        
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

        // create swapchain
        vk::SwapchainCreateInfoKHR info_swapchain {
            .surface = window._surface,
            .minImageCount = std::clamp<uint32_t>(
                3, // request triple buffering by default
                capabilities.minImageCount, 
                capabilities.maxImageCount == 0 ? UINT32_MAX : capabilities.maxImageCount),
            .imageFormat = _format,
            .imageColorSpace = color_space,
            .imageExtent = _extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eTransferDst,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queues._universal_i,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = vk::PresentModeKHR::eFifo,
            .clipped = true,
            .oldSwapchain = _swapchain,
        };
        vk::SwapchainKHR old_swapchain = _swapchain;
        _swapchain = device.createSwapchainKHR(info_swapchain);
        if (old_swapchain != nullptr && _images.size() > 0) device.destroySwapchainKHR(old_swapchain);
        _images.clear(); // old images were owned by previous swapchain

        // retrieve and wrap swapchain images
        std::vector<vk::Image> images = device.getSwapchainImagesKHR(_swapchain);
        for (vk::Image image: images) {
            Image::WrapInfo info_wrap {
                .image = image,
                .image_view = nullptr,
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
    void destroy_partial(vk::Device device) {
        for (auto& frame: _sync_frames) frame.destroy(device);
    }
    void destroy(vk::Device device) {
        if (_images.size() > 0) device.destroySwapchainKHR(_swapchain);
        destroy_partial(device);
    }
    
    void set_target_framerate(std::size_t fps) {
        double ns = 0;
        if (fps > 0) {
            double fps_d = (double)fps;
            double ms = 1.0 / fps_d * 1000.0;
            ns = ms * 1000000.0;
        }
        _target_frame_time = std::chrono::nanoseconds(static_cast<int64_t>(ns));
    }
    void resize(vk::PhysicalDevice physDevice, vk::Device device, Window& window, Queues& queues) {
        destroy_partial(device);
        init(physDevice, device, window, queues);
        fmt::println("Swapchain resized to: {}x{}", _extent.width, _extent.height);
    }
    void present(vk::Device device, Image& src_image, vk::Semaphore src_ready_to_read, vk::Semaphore src_ready_to_write) {
        // wait for this frame's fence to be signaled and reset it
        SyncFrame& frame = _sync_frames[_sync_frame_i++ % _sync_frames.size()];
        while (vk::Result::eTimeout == device.waitForFences(frame._ready_to_record, vk::True, UINT64_MAX));
        device.resetFences(frame._ready_to_record);

        // acquire image from swapchain
        uint32_t swap_index = 0;
        for (auto result = vk::Result::eTimeout; result == vk::Result::eTimeout;) {
            std::tie(result, swap_index) = device.acquireNextImageKHR(_swapchain, UINT64_MAX, frame._ready_to_write);
            // mark swapchain for resize, but still continue to present
            if (result == vk::Result::eSuboptimalKHR) {
                fmt::println("Swapchain image suboptimal");
                _resize_requested = true;
            }
            // abort if swapchain is out of date
            else
            if (result == vk::Result::eErrorOutOfDateKHR) {
                fmt::println("Swapchain image out of date");
                _resize_requested = true;
                return;
            }
        }
        
        // restart command buffer
        device.resetCommandPool(frame._command_pool);
        vk::CommandBuffer cmd = frame._command_buffer;
        cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        draw_imgui(cmd, src_image);
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
        std::array<vk::Semaphore, 2> wait_semaphores = { src_ready_to_read, frame._ready_to_write };
        std::array<vk::Semaphore, 2> sign_semaphores = { src_ready_to_write, frame._ready_to_read };
        std::array<vk::PipelineStageFlags, 2> wait_stages = { 
            vk::PipelineStageFlagBits::eTopOfPipe, 
            vk::PipelineStageFlagBits::eTopOfPipe 
        };
        vk::SubmitInfo info_submit {
            .waitSemaphoreCount = (uint32_t)wait_semaphores.size(),
            .pWaitSemaphores = wait_semaphores.data(),
            .pWaitDstStageMask = wait_stages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = (uint32_t)sign_semaphores.size(),
            .pSignalSemaphores = sign_semaphores.data(),
        };
        _presentation_queue.submit(info_submit, frame._ready_to_record);

        // present swapchain image
        vk::PresentInfoKHR presentInfo {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame._ready_to_read,
            .swapchainCount = 1,
            .pSwapchains = &_swapchain,
            .pImageIndices = &swap_index,
            .pResults = nullptr
        };
        wait_target_framerate();
        try {
            vk::Result result = _presentation_queue.presentKHR(presentInfo);
            if (result == vk::Result::eSuboptimalKHR) {
                fmt::println("Swapchain suboptimal");
                _resize_requested = true;
            }
        }
        catch (const vk::OutOfDateKHRError& e) {
            fmt::println("Swapchain out of date");
            _resize_requested = true;
            return;
        }
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
    void wait_target_framerate() {
        // keep track of how much time passed since last frame was presented
        auto new_timestamp = std::chrono::high_resolution_clock::now();
        std::chrono::nanoseconds diff = new_timestamp - _timestamp;

        // figure out if and how long needs to be waited
        if (_target_frame_time > diff) {
            std::this_thread::sleep_for(_target_frame_time - diff);
        }
        _timestamp = std::chrono::high_resolution_clock::now();
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
    std::chrono::duration<int64_t, std::nano> _target_frame_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> _timestamp;
};