#pragma once
#include <cstdint>
#include <cmath>
#include <vulkan/vulkan.hpp>
#include <fmt/base.h>
#include "core/queues.hpp"
#include "core/swapchain.hpp"
#include "core/pipeline.hpp"
#include "core/smaa.hpp"
#include "core/image.hpp"
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
        _smaa_area.destroy(device, vmalloc);
        _smaa_search.destroy(device, vmalloc);
        _smaa_edges.destroy(device, vmalloc);
        _smaa_weights.destroy(device, vmalloc);
        _smaa_output.destroy(device, vmalloc);
        // destroy pipelines
        _pipe_default.destroy(device);
        _pipe_cells.destroy(device);
        _pipe_smaa_edges.destroy(device);
        _pipe_smaa_weights.destroy(device);
        _pipe_smaa_blending.destroy(device);
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

        // optionally run SMAA
        if (Keys::pressed(SDLK_P)) _smaa_enabled = !_smaa_enabled;
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
        swapchain.present(device, *_final_image_p, _ready_to_read, _ready_to_write);
    }
    
private:
    void init_images(vk::Device device, vma::Allocator vmalloc, Queues& queues, vk::Extent2D extent) {
        // create image with 16 bits color depth
        _color.init({
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR16G16B16A16Sfloat,
            .extent { extent.width, extent.height, 1 },
            .usage = 
                vk::ImageUsageFlagBits::eColorAttachment | 
                vk::ImageUsageFlagBits::eTransferSrc | 
                vk::ImageUsageFlagBits::eSampled,
        });

        // create depth stencil image
        _depth_stencil.init(device, vmalloc, { extent.width, extent.height, 1 });

        // create SMAA output image with 16 bits color depth
        _smaa_output.init({
            .device = device, .vmalloc = vmalloc,
            .format = _color._format,
            .extent { extent.width, extent.height, 1 },
            .usage = 
                vk::ImageUsageFlagBits::eColorAttachment |
                vk::ImageUsageFlagBits::eTransferSrc |
                vk::ImageUsageFlagBits::eSampled
        });

        // create smaa edges and blend images
        _smaa_edges.init({
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8G8Unorm,
            .extent { extent.width, extent.height, 1 },
            .usage = 
                vk::ImageUsageFlagBits::eColorAttachment | 
                vk::ImageUsageFlagBits::eSampled,
        });
        _smaa_weights.init({
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8G8B8A8Unorm,
            .extent { extent.width, extent.height, 1 },
            .usage = 
                vk::ImageUsageFlagBits::eColorAttachment | 
                vk::ImageUsageFlagBits::eSampled,
        });

        // load smaa lookup textures
        _smaa_search.init({
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8Unorm,
            .extent = smaa::get_search_extent(),
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        });
        _smaa_search.load_texture(device, vmalloc, queues, smaa::get_search_tex());
        _smaa_area.init({
            .device = device, .vmalloc = vmalloc,
            .format = vk::Format::eR8G8Unorm,
            .extent = smaa::get_area_extent(),
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        });
        _smaa_area.load_texture(device, vmalloc, queues, smaa::get_area_tex());
        // transition smaa textures to their permanent layouts
        vk::CommandBuffer cmd = queues.oneshot_begin(device);
        Image::TransitionInfo info_transition {
            .cmd = cmd,
            .new_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .dst_stage = vk::PipelineStageFlagBits2::eFragmentShader,
            .dst_access = vk::AccessFlagBits2::eColorAttachmentRead
        };
        _smaa_search.transition_layout(info_transition);
        _smaa_area.transition_layout(info_transition);
        queues.oneshot_end(device, cmd);
    }
    void init_pipelines(vk::Device device, vk::Extent2D extent, Camera& camera) {
        // create graphics pipelines
        _pipe_default.init({
            .device = device, .extent = extent,
            .color_formats = { _color._format },
            .depth_format = _depth_stencil._format,
            .depth_write = vk::True, .depth_test = vk::True,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "defaults/default.vert", .fs_path = "defaults/default.frag",
        });
        _pipe_cells.init({
            .device = device, .extent = extent,
            .color_formats = { _color._format },
            .depth_format = _depth_stencil._format,
            .blend_enabled = vk::True,
            .depth_write = vk::False, .depth_test = vk::True,
            .poly_mode = vk::PolygonMode::eLine,
            .primitive_topology = vk::PrimitiveTopology::eLineStrip,
            .primitive_restart = true,
            .cull_mode = vk::CullModeFlagBits::eNone,
            .vs_path = "extra/cells.vert", .fs_path = "extra/cells.frag",
        });
        // write camera descriptor to pipelines
        _pipe_default.write_descriptor(device, 0, 0, camera._buffer);
        _pipe_cells.write_descriptor(device, 0, 0, camera._buffer);

        // create SMAA pipelines
        glm::aligned_vec4 SMAA_RT_METRICS = {
            1.0 / (double)extent.width,
            1.0 / (double)extent.height,
            (double)extent.width,
            (double)extent.height
        };
        std::array<vk::SpecializationMapEntry, 4> smaa_spec_entries {
            vk::SpecializationMapEntry { .constantID = 0, .offset = 0, .size = 4 },
            vk::SpecializationMapEntry { .constantID = 1, .offset = 4, .size = 4 },
            vk::SpecializationMapEntry { .constantID = 2, .offset = 8, .size = 4 },
            vk::SpecializationMapEntry { .constantID = 3, .offset = 12, .size = 4 }
        };
        vk::SpecializationInfo smaa_spec_info = {
            .mapEntryCount = smaa_spec_entries.size(),
            .pMapEntries = smaa_spec_entries.data(),
            .dataSize = sizeof(SMAA_RT_METRICS),
            .pData = &SMAA_RT_METRICS
        };
        _pipe_smaa_edges.init({
            .device = device, .extent = extent,
            .color_formats = { _smaa_edges._format },
            .stencil_format = _depth_stencil._format,
            .stencil_test = vk::True,
            .stencil_ops = {
                .failOp = vk::StencilOp::eKeep,
                .passOp = vk::StencilOp::eReplace,
                .compareOp = vk::CompareOp::eAlways,
                .compareMask = 0xff,
                .writeMask = 0xff,
                .reference = 1,
            },
            .vs_path = "smaa/edges.vert", .vs_spec = &smaa_spec_info,
            .fs_path = "smaa/edges.frag", .fs_spec = &smaa_spec_info,
        });
        _pipe_smaa_weights.init({
            .device = device, .extent = extent,
            .color_formats = { _smaa_weights._format },
            .stencil_format = _depth_stencil._format,
            .stencil_test = vk::True,
            .stencil_ops = {
                .failOp = vk::StencilOp::eKeep,
                .passOp = vk::StencilOp::eKeep,
                .compareOp = vk::CompareOp::eEqual,
                .compareMask = 0xff,
                .writeMask = 0xff,
                .reference = 1,
            },
            .vs_path = "smaa/weights.vert", .vs_spec = &smaa_spec_info,
            .fs_path = "smaa/weights.frag", .fs_spec = &smaa_spec_info,
        });
        _pipe_smaa_blending.init({
            .device = device, .extent = extent,
            .color_formats = { _color._format },
            .vs_path = "smaa/blending.vert", .vs_spec = &smaa_spec_info,
            .fs_path = "smaa/blending.frag", .fs_spec = &smaa_spec_info,
        });
        // update SMAA input texture descriptors
        _pipe_smaa_edges.write_descriptor(device, 0, 0, _color);
        _pipe_smaa_weights.write_descriptor(device, 0, 0, _smaa_area);
        _pipe_smaa_weights.write_descriptor(device, 0, 1, _smaa_search);
        _pipe_smaa_weights.write_descriptor(device, 0, 2, _smaa_edges);
        _pipe_smaa_blending.write_descriptor(device, 0, 0, _smaa_weights);
        _pipe_smaa_blending.write_descriptor(device, 0, 1, _color);
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

        auto& scene_data = scene._data;
        auto& mesh_main = scene._render_grey ? scene_data._mesh_main_grey._mesh : scene_data._mesh_main._mesh;
        _pipe_default.execute(cmd, mesh_main, _color, vk::AttachmentLoadOp::eClear, _depth_stencil, vk::AttachmentLoadOp::eClear);
        if (scene._render_subs) {
            _pipe_default.execute(cmd, scene_data._mesh_subs[scene._mesh_sub_i]._mesh, _color, vk::AttachmentLoadOp::eLoad);
        }
        
        // draw cells
        if (scene._render_grid) {
            _pipe_cells.execute(cmd, scene_data._grid._query_points, _color, vk::AttachmentLoadOp::eLoad, _depth_stencil, vk::AttachmentLoadOp::eLoad);
        }
        _final_image_p = &_color;
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
        _pipe_smaa_edges.execute(cmd, _smaa_edges, vk::AttachmentLoadOp::eClear, _depth_stencil, vk::AttachmentLoadOp::eLoad);

        // SMAA blending weight calculation
        _smaa_edges.transition_layout(info_transition_read);
        _smaa_weights.transition_layout(info_transition_write);
        _pipe_smaa_weights.execute(cmd, _smaa_weights, vk::AttachmentLoadOp::eClear, _depth_stencil, vk::AttachmentLoadOp::eLoad);

        // SMAA neighborhood blending
        _smaa_weights.transition_layout(info_transition_read);
        _smaa_output.transition_layout(info_transition_write);
        _pipe_smaa_blending.execute(cmd, _smaa_output, vk::AttachmentLoadOp::eClear);
        _final_image_p = &_smaa_output;
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
    DepthStencil _depth_stencil;
    Image _color;
    Image* _final_image_p = &_color;
    // SMAA
    Image _smaa_area;
    Image _smaa_search;
    Image _smaa_edges;
    Image _smaa_weights;
    Image _smaa_output;
    bool _smaa_enabled = true;

    // pipelines
    Pipeline::Graphics _pipe_default;
    Pipeline::Graphics _pipe_cells;
    // SMAA
    Pipeline::Graphics _pipe_smaa_edges;
    Pipeline::Graphics _pipe_smaa_weights;
    Pipeline::Graphics _pipe_smaa_blending;
};