#pragma once
#include <cstdint>
#include <cmath>
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
#include "core/queues.hpp"
#include "core/swapchain.hpp"
#include "core/pipeline.hpp"
#include "components/image.hpp"
#include "components/camera.hpp"
#include "components/scene.hpp"

class Renderer {
    // move this and swapchain thingy to its own header
    struct SyncFrame {
        void init(vk::Device device, Queues& queues) {
            vk::CommandPoolCreateInfo info_pool {
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queues._universal_i,
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
        // create SyncFrame objects
        for (auto& frame: _sync_frames) frame.init(device, queues);
        
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
        _color.init(info_image);

        // create depth image without stencil
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eD32Sfloat,
            .extent { extent.width, extent.height, 1 },
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
            .aspects = vk::ImageAspectFlagBits::eDepth
        };
        _depth.init(info_image);

        // create graphics pipelines
        Pipeline::Graphics::CreateInfo info_pipeline {
            .device = device, .extent = extent,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .depth_write = true, .depth_test = true,
            .vs_path = "defaults/default.vert", .fs_path = "defaults/default.frag",
        };
        _pipe_default.init(info_pipeline);
        // specifically for pointclouds and tsdf cells
        info_pipeline = {
            .device = device, .extent = extent,
            .poly_mode = vk::PolygonMode::ePoint,
            .primitive_topology = vk::PrimitiveTopology::ePointList,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .depth_write = false, .depth_test = true,
            .vs_path = "extra/scan_points.vert", .fs_path = "extra/scan_points.frag",
        };
        _pipe_scan_points.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .poly_mode = vk::PolygonMode::eLine,
            .primitive_topology = vk::PrimitiveTopology::eLineStrip,
            .primitive_restart = true,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .blend_enabled = true,
            .depth_write = false, .depth_test = true,
            .vs_path = "extra/cells.vert", .fs_path = "extra/cells.frag",
        };
        _pipe_cells.init(info_pipeline);

        // write camera descriptor to pipelines
        _pipe_default.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));
        _pipe_scan_points.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));
        _pipe_cells.write_descriptor(device, 0, 0, camera._buffer, sizeof(Camera::BufferData));

        smaa_init(device, vmalloc, extent);
    }
    void destroy(vk::Device device, vma::Allocator vmalloc) {
        for (auto& frame: _sync_frames) frame.destroy(device);
        _color.destroy(device, vmalloc);
        _depth.destroy(device, vmalloc);
        _pipe_default.destroy(device);
        _pipe_scan_points.destroy(device);
        _pipe_cells.destroy(device);
        smaa_destroy(device, vmalloc);
    }
    
    void resize(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera) { // todo: proper resize
        destroy(device, vmalloc);
        init(device, vmalloc, queues, extent, camera);
    }
    void render(vk::Device device, Swapchain& swapchain, Queues& queues, Scene& scene) {
        // wait for command buffer execution
        SyncFrame& frame = _sync_frames[_sync_frame_i++ % _sync_frames.size()];
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
        swapchain.present(device, _color, frame._timeline, frame._timeline_val);
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
        _color.transition_layout(info_transition);
        _pipe_default.execute(cmd, _color, _depth, scene._ply._mesh, true); // clear both color and depth

        // draw triangles
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .src_access = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _color.transition_layout(info_transition);
        _pipe_scan_points.execute(cmd, _color, _depth, scene._grid._scan_points, false);
        
        // draw cells
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .src_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .src_access = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
        };
        _color.transition_layout(info_transition);
        _pipe_cells.execute(cmd, _color, _depth, scene._grid._query_points, false);
    }
    
    void smaa_init(vk::Device device, vma::Allocator vmalloc, vk::Extent2D extent) {
        // create smaa edges and blend images
        Image::CreateInfo info_image {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR16G16B16A16Sfloat,
            .extent { extent.width, extent.height, 1 },
            .usage = 
                vk::ImageUsageFlagBits::eColorAttachment | 
                vk::ImageUsageFlagBits::eSampled
        };
        _smaa_edges.init(info_image);
        _smaa_blend.init(info_image);
        
        // create smaa pipeline
        Pipeline::Graphics::CreateInfo info_pipeline {
            .device = device, .extent = extent,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "smaa/edge_detection.vert", .fs_path = "smaa/edge_detection.frag",
        };
        _pipe_smaa_edges.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "smaa/blending_calc.vert", .fs_path = "smaa/blending_calc.frag",
        };
        _pipe_smaa_blend.init(info_pipeline);
    }
    void smaa_destroy(vk::Device device, vma::Allocator vmalloc) {
        _smaa_edges.destroy(device, vmalloc);
        _smaa_blend.destroy(device, vmalloc);
        _pipe_smaa_edges.destroy(device);
        _pipe_smaa_blend.destroy(device);
    }
    void smaa_execute() {

    }

private:
    std::array<SyncFrame, 2> _sync_frames; // double buffering
    uint32_t _sync_frame_i = 0;
    
    Image _color;
    Image _depth;
    Pipeline::Graphics _pipe_default;
    Pipeline::Graphics _pipe_scan_points;
    Pipeline::Graphics _pipe_cells;

    // smaa:
    Pipeline::Graphics _pipe_smaa_edges;
    Pipeline::Graphics _pipe_smaa_blend;
    Image _smaa_edges;
    Image _smaa_blend;
};