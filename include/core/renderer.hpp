#pragma once
#include <cstdint>
#include <cmath>
#include <vulkan/vulkan.hpp>
#include <fmt/base.h>
#include "core/queues.hpp"
#include "core/swapchain.hpp"
#include "core/pipeline.hpp"
#include "core/smaa.hpp"
#include "components/image.hpp"
#include "components/scene.hpp"

class Renderer {
public:
    void init(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera) {
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
        
        // create images and pipelines
        init_images(device, vmalloc, queues, extent);
        init_pipelines(device, extent, camera);
    }
    void destroy(vk::Device device, vma::Allocator vmalloc) {
        // destroy images
        _color.destroy(device, vmalloc);
        _depth_stencil.destroy(device, vmalloc);
        _smaa_area_tex.destroy(device, vmalloc);
        _smaa_search_tex.destroy(device, vmalloc);
        _smaa_edges.destroy(device, vmalloc);
        _smaa_blend.destroy(device, vmalloc);
        _smaa_output.destroy(device, vmalloc);
        // destroy pipelines
        _pipe_default.destroy(device);
        _pipe_cells.destroy(device);
        _pipe_smaa_edge_detection.destroy(device);
        _pipe_smaa_blend_weight_calc.destroy(device);
        _pipe_smaa_neigh_blending.destroy(device);
        // destroy command pools
        device.destroyCommandPool(_command_pool);
        // destroy synchronization objects
        device.destroyFence(_ready_to_record);
        device.destroySemaphore(_ready_to_write);
        device.destroySemaphore(_ready_to_read);
    }
    
    void resize(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent, Camera& camera) {
        destroy(device, vmalloc);
        init(device, vmalloc, queues, extent, camera);
    }
    void wait(vk::Device device) {
        // wait until the command buffer can be recorded
        while (vk::Result::eTimeout == device.waitForFences(_ready_to_record, vk::True, UINT64_MAX));
        device.resetFences(_ready_to_record);
    }
    void render(vk::Device device, Swapchain& swapchain, Queues& queues, Scene& scene) {
        // reset and record command buffer
        device.resetCommandPool(_command_pool, {});
        vk::CommandBuffer cmd = _command_buffer;
        cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        execute_pipes(cmd, scene);
        if (_smaa_enabled) execute_smaa(cmd);
        cmd.end();

        // submit command buffer
        vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        vk::SubmitInfo info_submit {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &_ready_to_write,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &_ready_to_read,
        };
        queues._universal.submit(info_submit, _ready_to_record);

        // present drawn image
        Image& output_image = _smaa_enabled ? _smaa_output : _color;
        swapchain.present(device, output_image, _ready_to_read, _ready_to_write);
    }
    
private:
    void init_images(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent) {
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

        // create SMAA output image with 16 bits color depth
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

        // load smaa lookup textures
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8Unorm,
            .extent = smaa::get_search_extent(),
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        };
        _smaa_search_tex.init(info_image);
        _smaa_search_tex.load_texture(device, vmalloc, queues, smaa::get_search_tex());
        info_image = {
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8G8Unorm,
            .extent = smaa::get_area_extent(),
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        };
        _smaa_area_tex.init(info_image);
        _smaa_area_tex.load_texture(device, vmalloc, queues, smaa::get_area_tex());
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
    }
    void init_pipelines(vk::Device device, vk::Extent2D extent, Camera& camera) {
        // create graphics pipelines
        Pipeline::Graphics::CreateInfo info_pipeline {
            .device = device, .extent = extent,
            .color_formats = { _color._format },
            .depth_format = _depth_stencil._format,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .depth_write = true, .depth_test = true,
            .vs_path = "defaults/default.vert", .fs_path = "defaults/default.frag",
        };
        _pipe_default.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .color_formats = { _color._format },
            .depth_format = _depth_stencil._format,
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
        _pipe_default.write_descriptor(device, 0, 0, camera._buffer);
        _pipe_cells.write_descriptor(device, 0, 0, camera._buffer);

        // create SMAA pipelines
        info_pipeline = {
            .device = device, .extent = extent,
            .color_formats = { _smaa_edges._format },
            .vs_path = "smaa/edge_detection.vert", .fs_path = "smaa/edge_detection.frag",
        };
        _pipe_smaa_edge_detection.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .color_formats = { _smaa_blend._format },
            .vs_path = "smaa/blend_weight_calc.vert", .fs_path = "smaa/blend_weight_calc.frag",
        };
        _pipe_smaa_blend_weight_calc.init(info_pipeline);
        info_pipeline = {
            .device = device, .extent = extent,
            .color_formats = { _color._format },
            .vs_path = "smaa/neigh_blending.vert", .fs_path = "smaa/neigh_blending.frag",
        };
        _pipe_smaa_neigh_blending.init(info_pipeline);
        // update SMAA input texture descriptors
        _pipe_smaa_edge_detection.write_descriptor(device, 0, 0, _color);
        _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 0, _smaa_edges);
        _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 1, _smaa_area_tex);
        _pipe_smaa_blend_weight_calc.write_descriptor(device, 0, 2, _smaa_search_tex);
        _pipe_smaa_neigh_blending.write_descriptor(device, 0, 0, _color);
        _pipe_smaa_neigh_blending.write_descriptor(device, 0, 1, _smaa_blend);
    }
    
    void execute_pipes(vk::CommandBuffer cmd, Scene& scene) {
        // draw scan points
        Image::TransitionInfo info_transition;
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite
        };
        _color.transition_layout(info_transition);
        info_transition = {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            .dst_access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
        };
        _depth_stencil.transition_layout(info_transition);
        auto& mesh_main = scene._render_grey ? scene._mesh_main_grey._mesh : scene._mesh_main._mesh;
        _pipe_default.execute(cmd, mesh_main, _color, vk::AttachmentLoadOp::eClear, _depth_stencil, vk::AttachmentLoadOp::eClear);
        if (scene._render_subs) {
            _pipe_default.execute(cmd, scene._mesh_subs[scene._mesh_sub_i]._mesh, _color, vk::AttachmentLoadOp::eLoad);
        }
        
        // draw cells
        if (scene._render_grid){
            _pipe_cells.execute(cmd, scene._grid._query_points, _color, vk::AttachmentLoadOp::eLoad, _depth_stencil, vk::AttachmentLoadOp::eLoad);
        }
    }
    void execute_smaa(vk::CommandBuffer cmd) {
        Image::TransitionInfo info_transition_read {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
            .dst_access = vk::AccessFlagBits2::eShaderSampledRead
        };
        Image::TransitionInfo info_transition_write {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eColorAttachmentOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentWrite
        };
        // SMAA edge detection
        _color.transition_layout(info_transition_read);
        _smaa_edges.transition_layout(info_transition_write);
        _pipe_smaa_edge_detection.execute(cmd, _smaa_edges, vk::AttachmentLoadOp::eClear);

        // SMAA blending weight calculation
        _smaa_edges.transition_layout(info_transition_read);
        _smaa_blend.transition_layout(info_transition_write);
        _pipe_smaa_blend_weight_calc.execute(cmd, _smaa_blend, vk::AttachmentLoadOp::eClear);

        // SMAA neighborhood blending
        _smaa_blend.transition_layout(info_transition_read);
        _smaa_output.transition_layout(info_transition_write);
        _pipe_smaa_neigh_blending.execute(cmd, _smaa_output, vk::AttachmentLoadOp::eClear);
    }

private:
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
    // SMAA todo: stencil optimizations
    Image _smaa_area_tex; // constant lookup tex
    Image _smaa_search_tex; // constant lookup tex
    Image _smaa_edges; // intermediate SMAA output
    Image _smaa_blend; // intermediate SMAA output
    Image _smaa_output; // final SMAA output
    bool _smaa_enabled = false;

    // pipelines
    Pipeline::Graphics _pipe_default;
    Pipeline::Graphics _pipe_cells;
    // SMAA
    Pipeline::Graphics _pipe_smaa_edge_detection;
    Pipeline::Graphics _pipe_smaa_blend_weight_calc;
    Pipeline::Graphics _pipe_smaa_neigh_blending;
};