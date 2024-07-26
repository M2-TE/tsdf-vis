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
#include "components/scene.hpp"

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
    void init(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera) {
        // create FrameData objects
        for (auto& frame: _frames) frame.init(device, queues);
        
        // create image with 16 bits color depth
        Image::CreateInfo info_image {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR16G16B16A16Sfloat,
            .extent { extent.width, extent.height, 1 },
            .usage = 
                vk::ImageUsageFlagBits::eColorAttachment | 
                vk::ImageUsageFlagBits::eTransferSrc | 
                vk::ImageUsageFlagBits::eStorage
        };
        _dst_image.init(info_image);

        // create depth image without stencil
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eD32Sfloat,
            .extent { extent.width, extent.height, 1 },
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            .aspects = vk::ImageAspectFlagBits::eDepth
        };
        _depth_image.init(info_image);

        // create graphics pipelines
        Pipeline::Graphics::CreateInfo info_pipeline {
            .device = device, .extent = extent,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "default.vert", .fs_path = "default.frag",
        };
        _pipe_default.init(info_pipeline);
        // specifically for pointclouds and tsdf cells
        info_pipeline = {
            .device = device, .extent = extent,
            .poly_mode = vk::PolygonMode::ePoint,
            .primitive_topology = vk::PrimitiveTopology::ePointList,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "scan_points.vert", .fs_path = "scan_points.frag",
        };
        _pipe_scan_points.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .poly_mode = vk::PolygonMode::eLine,
            .primitive_topology = vk::PrimitiveTopology::eLineStrip,
            .primitive_restart = true,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .blend_enabled = true,
            .vs_path = "cells.vert", .fs_path = "cells.frag",
        };
        _pipe_cells.init(info_pipeline);

        // write camera descriptor to pipelines
        _pipe_default.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));
        _pipe_scan_points.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));
        _pipe_cells.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));
    }
    void destroy(vk::Device device, vma::Allocator vmalloc) {
        for (auto& frame: _frames) frame.destroy(device);
        _dst_image.destroy(device, vmalloc);
        _depth_image.destroy(device, vmalloc);
        _pipe_default.destroy(device);
        _pipe_scan_points.destroy(device);
        _pipe_cells.destroy(device);
    }
    
    void resize(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera) { // todo: proper resize
        destroy(device, vmalloc);
        init(device, vmalloc, queues, extent, camera);
    }
    void render(vk::Device device, Swapchain& swapchain, Queues& queues, Scene& scene) {
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
        execute_pipes(device, cmd, scene);
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
    void execute_pipes(vk::Device device, vk::CommandBuffer cmd, Scene& scene) {
        // draw scan points
        Image::TransitionInfo info_transition;
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eAllCommands,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .src_access = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _dst_image.transition_layout(info_transition);
        _pipe_scan_points.execute(cmd, _dst_image, scene._grid._scan_points, true);

        // draw triangles
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .src_access = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _dst_image.transition_layout(info_transition);
        _pipe_default.execute(cmd, _dst_image, scene._ply._mesh, false);
        
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
        _pipe_cells.execute(cmd, _dst_image, scene._grid._query_points, false);
    }
    
private:
    std::array<FrameData, 2> _frames; // double buffering
    uint32_t _sync_frame = 0;
    
    Image _dst_image;
    Image _depth_image;
    Pipeline::Graphics _pipe_default;
    Pipeline::Graphics _pipe_scan_points;
    Pipeline::Graphics _pipe_cells;
};