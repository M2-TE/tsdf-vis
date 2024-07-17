#pragma once
#include <cstdint>
//
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
//
#include "image.hpp"
#include "queues.hpp"
#include "swapchain.hpp"

class Renderer {
    struct FrameData {
        void init(vk::Device device, Queues& queues) {
            vk::CommandPoolCreateInfo info_pool {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queues.i_graphics,
            };
            _command_pool = device.createCommandPool(info_pool);

            vk::CommandBufferAllocateInfo info_buffer {
                .commandPool = _command_pool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            _command_buffer = device.allocateCommandBuffers(info_buffer).front();
            
            vk::SemaphoreTypeCreateInfo info_sema_type {
                .semaphoreType = vk::SemaphoreType::eTimeline,
                .initialValue = 1,  
            };
            vk::SemaphoreCreateInfo info_sema { .pNext = &info_sema_type };
            _timeline = device.createSemaphore(info_sema);
            _timeline_val = info_sema_type.initialValue;
        }
        void destroy(vk::Device device) {
            device.destroyCommandPool(_command_pool);
            device.destroySemaphore(_timeline);
        }
        void reset_semaphore(vk::Device device) {
            vk::SemaphoreTypeCreateInfo info_sema_type {
                .semaphoreType = vk::SemaphoreType::eTimeline,
                .initialValue = 0,  
            };
            vk::SemaphoreCreateInfo info_sema { .pNext = &info_sema_type };
            device.destroySemaphore(_timeline);
            _timeline = device.createSemaphore(info_sema);
            _timeline_val = info_sema_type.initialValue;
        }
        
        vk::CommandPool _command_pool;
        vk::CommandBuffer _command_buffer;
        vk::Semaphore _timeline;
        uint64_t _timeline_val;
    };
public:
    void init(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent) {
        // create FrameData objects
        for (auto& frame: _frames) frame.init(device, queues);
        
        // create image with 16 bits color depth
        vk::ImageUsageFlags usage = 
            vk::ImageUsageFlagBits::eColorAttachment | 
            vk::ImageUsageFlagBits::eTransferSrc | 
            vk::ImageUsageFlagBits::eStorage;
        Image::CreateInfo info_image {
            .device = device,
            .vmalloc = vmalloc,
            .format = vk::Format::eR16G16B16A16Sfloat,
            .extent { extent.width, extent.height, 1 },
            .usage = usage
        };
        _dst_image.init(info_image);
        
        // // create shader pipeline
        // computePipe.init(device, "gradient.comp");
        // computePipe.write_descriptor(device, 0, 0, image);

        // // create graphics pipeline
        // graphicsPipe.init(device, extent, "default.vert", "default.frag");
        // graphicsPipe.write_descriptor(device, 0, 0, camera.buffer, sizeof(Camera::BufferData));
    }
    void destroy(vk::Device device, vma::Allocator vmalloc) {
        for (auto& frame: _frames) frame.destroy(device);
        _dst_image.destroy(device, vmalloc);
        // computePipe.destroy(device);
        // graphicsPipe.destroy(device);
    }
    void resize(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent) { // todo: proper resize
        destroy(device, vmalloc);
        init(device, vmalloc, queues, extent);
    }
    void render(vk::Device device, Swapchain& swapchain, Queues& queues) {
        // wait for command buffer execution
        FrameData& frame = _frames[_sync_frame++ % _frames.size()];
        vk::SemaphoreWaitInfo info_sema_wait {
            .semaphoreCount = 1,
            .pSemaphores = &frame._timeline,
            .pValues = &frame._timeline_val,  
        };
        for (vk::Result result = vk::Result::eTimeout; result == vk::Result::eTimeout;) {
            result = device.waitSemaphores(info_sema_wait, UINT64_MAX);
        }
        frame.reset_semaphore(device);
        
        // record command buffer
        vk::CommandBuffer cmd = frame._command_buffer;
        vk::CommandBufferBeginInfo info_cmd_begin {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        cmd.begin(info_cmd_begin);
        draw(device, cmd);
        cmd.end();
        
        // submit command buffer
        vk::TimelineSemaphoreSubmitInfo info_timeline {
            .signalSemaphoreValueCount = 1,
            .pSignalSemaphoreValues = &(++frame._timeline_val),
        };
        vk::SubmitInfo info_submit {
            .pNext = &info_timeline,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &frame._timeline,
        };
        queues.graphics.submit(info_submit);

        // present drawn image
        swapchain.present(device, _dst_image, frame._timeline, frame._timeline_val);
    }
    
private:
    void draw(vk::Device, vk::CommandBuffer) {
        // todo
    }
    
private:
    std::array<FrameData, 2> _frames; // double buffering
    uint32_t _sync_frame = 0;
    
    Image _dst_image;
    // Pipeline::Graphics _pipe_points;
    // Pipeline::Graphics _pipe_lines;
    // Pipeline::Graphics _pipe_triangles;
    // Pipeline::Compute _pipe_compute;
    
};