#pragma once
#include <cstdint>
#include <cmath>
//
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
//
#include "core/queues.hpp"
#include "core/swapchain.hpp"
#include "core/pipeline.hpp"
#include "components/image.hpp"
#include "components/camera.hpp"
#include "components/grid.hpp"

class Renderer {
    // move this and swapchain thingy to its own header
    struct FrameData {
        void init(vk::Device device, Queues& queues) {
            vk::CommandPoolCreateInfo info_pool {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queues._family_universal,
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
    void init(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera, Grid& grid) {
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

        // create graphics pipelines
        _pipe_scan_points.init(device, extent, "scan_points.vert", "scan_points.frag", vk::PolygonMode::ePoint, false);
        _pipe_scan_points.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));
        _pipe_query_points.init(device, extent, "query_points.vert", "query_points.frag", vk::PolygonMode::ePoint, false);
        _pipe_query_points.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));
        _pipe_cells.init(device, extent, "cells.vert", "cells.frag", vk::PolygonMode::eLine, true);
        _pipe_cells.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));
    }
    void destroy(vk::Device device, vma::Allocator vmalloc) {
        for (auto& frame: _frames) frame.destroy(device);
        _dst_image.destroy(device, vmalloc);
        _pipe_scan_points.destroy(device);
        _pipe_query_points.destroy(device);
        _pipe_cells.destroy(device);
    }
    
    void resize(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera, Grid& grid) { // todo: proper resize
        destroy(device, vmalloc);
        init(device, vmalloc, queues, extent, camera, grid);
    }
    void render(vk::Device device, Swapchain& swapchain, Queues& queues, Grid& grid) {
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
        execute_pipes(device, cmd, grid);
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
        queues._universal.submit(info_submit);

        // present drawn image
        swapchain.present(device, _dst_image, frame._timeline, frame._timeline_val);
    }
    
private:
    void execute_pipes(vk::Device device, vk::CommandBuffer cmd, Grid& grid) {
        // draw scan points
        Image::TransitionInfo info_transition {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eAllCommands,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .src_access = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _dst_image.transition_layout(info_transition);      
        _pipe_scan_points.execute(cmd, _dst_image, grid._scan_points, true);
        
        // draw cells
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .src_access = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
        };
        _dst_image.transition_layout(info_transition);
        _pipe_cells.execute(cmd, _dst_image, grid._query_points, false);
    }
    
private:
    std::array<FrameData, 2> _frames; // double buffering
    uint32_t _sync_frame = 0;
    
    Image _dst_image;
    Pipeline::Graphics _pipe_scan_points;
    Pipeline::Graphics _pipe_query_points;
    Pipeline::Graphics _pipe_cells;
};