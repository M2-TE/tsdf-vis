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

// temporary stuff for SMAA
#include "AreaTex.h"
#include "SearchTex.h"

class Renderer {
    // move this and swapchain thingy to its own header
    struct SyncFrame {
        void init(vk::Device device, Queues& queues) {
            vk::CommandPoolCreateInfo info_pool {
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
                vk::ImageUsageFlagBits::eSampled
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
        smaa_init(device, vmalloc, queues, extent);
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
    
    void resize(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera) {
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
        device.resetCommandPool(frame._command_pool, {});
        vk::CommandBuffer cmd = frame._command_buffer;
        cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        execute_pipes(device, cmd, scene);
        if (_smaa_enabled) smaa_execute(cmd);
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
        swapchain.present(device, _smaa_enabled ? _smaa_output : _color, frame._timeline, frame._timeline_val);
    }
    
private:
    void execute_pipes(vk::Device device, vk::CommandBuffer cmd, Scene& scene) {
        // draw scan points
        Image::TransitionInfo info_transition;
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _color.transition_layout(info_transition);
        _pipe_default.execute(cmd, scene._ply._mesh, _color, vk::AttachmentLoadOp::eClear, _depth, vk::AttachmentLoadOp::eClear);

        // draw points
        _pipe_scan_points.execute(cmd, scene._grid._scan_points, _color, vk::AttachmentLoadOp::eLoad, _depth, vk::AttachmentLoadOp::eLoad);
        
        // draw cells
        _pipe_cells.execute(cmd, scene._grid._query_points, _color, vk::AttachmentLoadOp::eLoad, _depth, vk::AttachmentLoadOp::eLoad);
    }
    
    void smaa_init(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent) {
        // create output image with 16 bits color depth
        Image::CreateInfo info_image {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR16G16B16A16Sfloat,
            .extent { extent.width, extent.height, 1 },
            .usage = 
                vk::ImageUsageFlagBits::eColorAttachment |
                vk::ImageUsageFlagBits::eTransferSrc |
                vk::ImageUsageFlagBits::eSampled
        };
        _smaa_output.init(info_image);

        // create smaa edges and blend images
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR16G16B16A16Sfloat,
            .extent { extent.width, extent.height, 1 },
            .usage = 
                vk::ImageUsageFlagBits::eColorAttachment | 
                vk::ImageUsageFlagBits::eSampled
        };
        _smaa_edges.init(info_image);
        _smaa_blend.init(info_image);

        // load smaa textures
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8Unorm,
            .extent { SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1 },
            .usage = vk::ImageUsageFlagBits::eSampled,
        };
        _smaa_search_tex.init(info_image);
        _smaa_search_tex.load_texture(vmalloc, std::span(reinterpret_cast<const std::byte*>(searchTexBytes), sizeof(searchTexBytes)));
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8G8Unorm,
            .extent { AREATEX_WIDTH, AREATEX_HEIGHT, 1 },
            .usage = vk::ImageUsageFlagBits::eSampled,
        };
        _smaa_area_tex.init(info_image);
        _smaa_area_tex.load_texture(vmalloc, std::span(reinterpret_cast<const std::byte*>(areaTexBytes), sizeof(areaTexBytes)));

        // transition smaa textures to their permanent layouts
        vk::CommandBuffer cmd = queues.oneshot_begin(device);
        Image::TransitionInfo info_transition {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentRead
        };
        _smaa_search_tex.transition_layout(info_transition);
        _smaa_area_tex.transition_layout(info_transition);
        queues.oneshot_end(device, cmd);
        
        // create smaa pipeline
        Pipeline::Graphics::CreateInfo info_pipeline {
            .device = device, .extent = extent,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "smaa/edge_detection.vert", .fs_path = "smaa/edge_detection.frag",
        };
        _pipe_smaa_edge_detection.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "smaa/blend_weight_calc.vert", .fs_path = "smaa/blend_weight_calc.frag",
        };
        _pipe_smaa_blend_weight_calc.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "smaa/neigh_blending.vert", .fs_path = "smaa/neigh_blending.frag",
        };
        _pipe_smaa_neigh_blending.init(info_pipeline);

        // update texture descriptors
        _pipe_smaa_edge_detection.write_descriptor(device, 0, 0, _color);
        _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 0, _smaa_edges);
        _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 1, _smaa_area_tex);
        _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 2, _smaa_search_tex);
        _pipe_smaa_neigh_blending.write_descriptor(device, 0, 0, _smaa_blend);
        _pipe_smaa_neigh_blending.write_descriptor(device, 0, 1, _color);
    }
    void smaa_destroy(vk::Device device, vma::Allocator vmalloc) {
        _smaa_search_tex.destroy(device, vmalloc);
        _smaa_area_tex.destroy(device, vmalloc);
        _smaa_output.destroy(device, vmalloc);
        _smaa_edges.destroy(device, vmalloc);
        _smaa_blend.destroy(device, vmalloc);
        _pipe_smaa_edge_detection.destroy(device);
        _pipe_smaa_blend_weight_calc.destroy(device);
        _pipe_smaa_neigh_blending.destroy(device);
    }
    void smaa_execute(vk::CommandBuffer cmd) {
        // SMAA edge detection
        Image::TransitionInfo info_transition;
        info_transition = { // input
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentRead
        };
        _color.transition_layout(info_transition);
        info_transition = { // output
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _smaa_edges.transition_layout(info_transition);
        _pipe_smaa_edge_detection.execute(cmd, _smaa_edges, vk::AttachmentLoadOp::eClear);

        // SMAA blending weight calculation
        info_transition = { // input
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentRead
        };
        _smaa_edges.transition_layout(info_transition);
        info_transition = { // output
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _smaa_blend.transition_layout(info_transition);
        _pipe_smaa_blend_weight_calc.execute(cmd, _smaa_blend, vk::AttachmentLoadOp::eClear);

        // SMAA neighborhood blending
        info_transition = { // input
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentRead
        };
        _smaa_blend.transition_layout(info_transition);
        info_transition = { // output
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _smaa_output.transition_layout(info_transition);
        _pipe_smaa_neigh_blending.execute(cmd, _smaa_output, vk::AttachmentLoadOp::eDontCare);
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
    Pipeline::Graphics _pipe_smaa_edge_detection;
    Pipeline::Graphics _pipe_smaa_blend_weight_calc;
    Pipeline::Graphics _pipe_smaa_neigh_blending;
    Image _smaa_search_tex; // constant
    Image _smaa_area_tex; // constant
    Image _smaa_edges; // intermediate SMAA output // todo: reduce to 1/2 channels
    Image _smaa_blend; // intermediate SMAA output
    Image _smaa_output; // final SMAA output
    bool _smaa_enabled = false;
};