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

// todo: remove timeline semaphore from vulkan features
class Renderer {
    struct Frame {
        void init(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent) {
            // allocate single command pool and buffer pair
            _command_pool = device.createCommandPool({ .queueFamilyIndex = queues._universal_i });
            vk::CommandBufferAllocateInfo bufferInfo {
                .commandPool = _command_pool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            _command_buffer = device.allocateCommandBuffers(bufferInfo).front();

            // allocate semaphores and fences
            _ready_to_record = device.createFence({ .flags = vk::FenceCreateFlagBits::eSignaled });
            _ready_to_write = device.createSemaphore({});
            _ready_to_read = device.createSemaphore({});
            // create dummy submission to initialize _ready_to_write
            auto cmd = queues.oneshot_begin(device);
            queues.oneshot_end(device, cmd, _ready_to_write);

            // create image with 16 bits color depth
            Image::CreateInfo info_image {
                .device = device, .vmalloc = vmalloc,
                .format = vk::Format::eR16G16B16A16Sfloat,
                .extent { extent.width, extent.height, 1 },
                .usage = 
                    vk::ImageUsageFlagBits::eColorAttachment | 
                    vk::ImageUsageFlagBits::eTransferSrc | 
                    vk::ImageUsageFlagBits::eSampled,
            };
            _color.init(info_image);

            // create depth stencil image
            _depth_stencil.init(device, vmalloc, { extent.width, extent.height, 1 });

            // create output image with 16 bits color depth
            info_image = {
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
                .format = vk::Format::eR16G16Sfloat,
                .extent { extent.width, extent.height, 1 },
                .usage = 
                    vk::ImageUsageFlagBits::eColorAttachment | 
                    vk::ImageUsageFlagBits::eSampled,
            };
            _smaa_edges.init(info_image);
            info_image = {
                .device = device, .vmalloc = vmalloc,
                .format = vk::Format::eR16G16B16A16Sfloat,
                .extent { extent.width, extent.height, 1 },
                .usage = 
                    vk::ImageUsageFlagBits::eColorAttachment | 
                    vk::ImageUsageFlagBits::eSampled,
            };
            _smaa_blend.init(info_image);
        }
        void destroy(vk::Device device, vma::Allocator vmalloc) {
            device.destroyCommandPool(_command_pool);
            device.destroyFence(_ready_to_record);
            device.destroySemaphore(_ready_to_write);
            device.destroySemaphore(_ready_to_read);
            _color.destroy(device, vmalloc);
            _depth_stencil.destroy(device, vmalloc);
            _smaa_edges.destroy(device, vmalloc);
            _smaa_blend.destroy(device, vmalloc);
            _smaa_output.destroy(device, vmalloc);
        }
        
        // synchronization
        vk::Fence _ready_to_record;
        vk::Semaphore _ready_to_write;
        vk::Semaphore _ready_to_read;

        // command recording
        vk::CommandPool _command_pool;
        vk::CommandBuffer _command_buffer;

        // Images
        Image _color;
        DepthStencil _depth_stencil;
        // SMAA
        Image _smaa_edges; // intermediate SMAA output
        Image _smaa_blend; // intermediate SMAA output
        Image _smaa_output; // final SMAA output
    };
public:
    void init(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera) {
        // create multiple frames in flight
        for (auto& frame: _frames) frame.init(device, vmalloc, queues, extent);

        // create graphics pipelines
        Pipeline::Graphics::CreateInfo info_pipeline {
            .device = device, .extent = extent,
            .color_formats = { _frames[0]._color._format },
            .depth_format = _frames[0]._depth_stencil._format,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .depth_write = true, .depth_test = true,
            .vs_path = "defaults/default.vert", .fs_path = "defaults/default.frag",
        };
        _pipe_default.init(info_pipeline);
        // specifically for pointclouds and tsdf cells
        info_pipeline = {
            .device = device, .extent = extent,
            .color_formats = { _frames[0]._color._format },
            .depth_format = _frames[0]._depth_stencil._format,
            .poly_mode = vk::PolygonMode::ePoint,
            .primitive_topology = vk::PrimitiveTopology::ePointList,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .depth_write = false, .depth_test = true,
            .vs_path = "extra/scan_points.vert", .fs_path = "extra/scan_points.frag",
        };
        _pipe_scan_points.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .color_formats = { _frames[0]._color._format },
            .depth_format = _frames[0]._depth_stencil._format,
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
        for (auto& frame: _frames) frame.destroy(device, vmalloc);
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
        Frame& frame = _frames[_frame_index++ % _frames.size()];

        // wait until the command buffer can be recorded
        while (vk::Result::eTimeout == device.waitForFences(frame._ready_to_record, vk::True, UINT64_MAX));
        device.resetFences(frame._ready_to_record);
        
        // reset and record command buffer
        device.resetCommandPool(frame._command_pool, {});
        vk::CommandBuffer cmd = frame._command_buffer;
        cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        execute_pipes(device, cmd, scene, frame);
        if (_smaa_enabled) smaa_execute(cmd);
        cmd.end();
        
        // submit command buffer
        vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        vk::SubmitInfo info_submit {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame._ready_to_write,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &frame._ready_to_read,
        };
        queues._universal.submit(info_submit, frame._ready_to_record);

        // present drawn image
        Image& output_image = _smaa_enabled ? frame._smaa_output : frame._color;
        swapchain.present(device, output_image, frame._ready_to_read, frame._ready_to_write);
    }
    
private:
    void execute_pipes(vk::Device device, vk::CommandBuffer cmd, Scene& scene, Frame& frame) {
        // draw scan points
        Image::TransitionInfo info_transition;
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
        };
        frame._color.transition_layout(info_transition);
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            .dst_access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        };
        frame._depth_stencil.transition_layout(info_transition);
        _pipe_default.execute(cmd, scene._ply._mesh, frame._color, vk::AttachmentLoadOp::eClear, frame._depth_stencil, vk::AttachmentLoadOp::eClear);

        // draw points
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
        };
        frame._color.transition_layout(info_transition);
        _pipe_scan_points.execute(cmd, scene._grid._scan_points, frame._color, vk::AttachmentLoadOp::eLoad, frame._depth_stencil, vk::AttachmentLoadOp::eLoad);
        
        // draw cells
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead
        };
        frame._color.transition_layout(info_transition);
        _pipe_cells.execute(cmd, scene._grid._query_points, frame._color, vk::AttachmentLoadOp::eLoad, frame._depth_stencil, vk::AttachmentLoadOp::eLoad);
    }
    
    void smaa_init(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent) {
        // load smaa textures
        Image::CreateInfo info_image;
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8Unorm,
            .extent { SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1 },
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        };
        _smaa_search_tex.init(info_image);
        _smaa_search_tex.load_texture(device, vmalloc, queues, std::span(reinterpret_cast<const std::byte*>(searchTexBytes), sizeof(searchTexBytes)));
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8G8Unorm,
            .extent { AREATEX_WIDTH, AREATEX_HEIGHT, 1 },
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        };
        _smaa_area_tex.init(info_image);
        _smaa_area_tex.load_texture(device, vmalloc, queues, std::span(reinterpret_cast<const std::byte*>(areaTexBytes), sizeof(areaTexBytes)));

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
            .color_formats = { _frames[0]._smaa_edges._format },
            .vs_path = "smaa/edge_detection.vert", .fs_path = "smaa/edge_detection.frag",
        };
        _pipe_smaa_edge_detection.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .color_formats = { _frames[0]._smaa_blend._format },
            .vs_path = "smaa/blend_weight_calc.vert", .fs_path = "smaa/blend_weight_calc.frag",
        };
        _pipe_smaa_blend_weight_calc.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .color_formats = { _frames[0]._color._format },
            .vs_path = "smaa/neigh_blending.vert", .fs_path = "smaa/neigh_blending.frag",
        };
        _pipe_smaa_neigh_blending.init(info_pipeline);

        // TODO: need separate pipelines for each frame
        // update texture descriptors
        // _pipe_smaa_edge_detection.write_descriptor(device, 0, 0, _color);
        // _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 0, _smaa_edges);
        // _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 1, _smaa_area_tex);
        // _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 2, _smaa_search_tex);
        // _pipe_smaa_neigh_blending.write_descriptor(device, 0, 0, _color);
        // _pipe_smaa_neigh_blending.write_descriptor(device, 0, 1, _smaa_blend);
    }
    void smaa_destroy(vk::Device device, vma::Allocator vmalloc) {
        _smaa_search_tex.destroy(device, vmalloc);
        _smaa_area_tex.destroy(device, vmalloc);
        _pipe_smaa_edge_detection.destroy(device);
        _pipe_smaa_blend_weight_calc.destroy(device);
        _pipe_smaa_neigh_blending.destroy(device);
    }
    void smaa_execute(vk::CommandBuffer cmd) {
        // // SMAA edge detection
        // Image::TransitionInfo info_transition;
        // info_transition = { // input
        //     .cmd = cmd,
        //     .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
        //     .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
        //     .dst_access = vk::AccessFlagBits2::eShaderSampledRead
        // };
        // _color.transition_layout(info_transition);
        // info_transition = { // output
        //     .cmd = cmd,
        //     .new_layout = vk::ImageLayout::eAttachmentOptimal,
        //     .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        //     .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        // };
        // _smaa_edges.transition_layout(info_transition);
        // _pipe_smaa_edge_detection.execute(cmd, _smaa_edges, vk::AttachmentLoadOp::eClear);

        // // SMAA blending weight calculation
        // info_transition = { // input
        //     .cmd = cmd,
        //     .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
        //     .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
        //     .dst_access = vk::AccessFlagBits2::eShaderSampledRead
        // };
        // _smaa_edges.transition_layout(info_transition);
        // info_transition = { // output
        //     .cmd = cmd,
        //     .new_layout = vk::ImageLayout::eAttachmentOptimal,
        //     .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        //     .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        // };
        // _smaa_blend.transition_layout(info_transition);
        // _pipe_smaa_blend_weight_calc.execute(cmd, _smaa_blend, vk::AttachmentLoadOp::eClear);

        // // SMAA neighborhood blending
        // info_transition = { // input
        //     .cmd = cmd,
        //     .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
        //     .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
        //     .dst_access = vk::AccessFlagBits2::eShaderSampledRead
        // };
        // _smaa_blend.transition_layout(info_transition);
        // info_transition = { // output
        //     .cmd = cmd,
        //     .new_layout = vk::ImageLayout::eAttachmentOptimal,
        //     .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        //     .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        // };
        // _smaa_output.transition_layout(info_transition);
        // _pipe_smaa_neigh_blending.execute(cmd, _smaa_output, vk::AttachmentLoadOp::eClear);
    }

private:
    std::array<Frame, 2> _frames; // double buffering
    uint32_t _frame_index = 0;
    Pipeline::Graphics _pipe_default;
    Pipeline::Graphics _pipe_scan_points;
    Pipeline::Graphics _pipe_cells;
    // SMAA
    // todo: stencil optimizations
    // todo: separate sync for smaa images, as these dont need multiple frames in flight
    Pipeline::Graphics _pipe_smaa_edge_detection;
    Pipeline::Graphics _pipe_smaa_blend_weight_calc;
    Pipeline::Graphics _pipe_smaa_neigh_blending;
    Image _smaa_search_tex; // constant
    Image _smaa_area_tex; // constant
    bool _smaa_enabled = false;
};