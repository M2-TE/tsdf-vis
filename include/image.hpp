#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>

struct Image {
    struct CreateInfo {
        vk::Device device;
        vma::Allocator vmalloc;
        vk::Format format;
        vk::Extent3D extent;
        vk::ImageUsageFlags usage;
        vk::ImageAspectFlags aspects = vk::ImageAspectFlagBits::eColor;
    };
    struct TransitionInfo {
        vk::CommandBuffer cmd;
        vk::ImageLayout new_layout;
        vk::PipelineStageFlags2 src_stage = vk::PipelineStageFlagBits2::eAllCommands;
        vk::PipelineStageFlags2 dst_stage = vk::PipelineStageFlagBits2::eAllCommands;
        vk::AccessFlags2 src_access = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
        vk::AccessFlags2 dst_access = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
    };
    void init(CreateInfo info) {
        _format = info.format;
        _extent = info.extent;
        _aspects = info.aspects;
        // create image
        vk::ImageCreateInfo info_image {
            .imageType = vk::ImageType::e2D,
            .format = _format,
            .extent = _extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = info.usage,
        };
        vma::AllocationCreateInfo info_alloc {
            .flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
            .usage = vma::MemoryUsage::eAutoPreferDevice,
            .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
        };
        std::tie(_image, _allocation) = info.vmalloc.createImage(info_image, info_alloc);
        
        // create image view
        vk::ImageViewCreateInfo info_view {
            .image = _image,
            .viewType = vk::ImageViewType::e2D,
            .format = _format,
            .subresourceRange {
                .aspectMask = _aspects,
                .baseMipLevel = 0,
                .levelCount = vk::RemainingMipLevels,
                .baseArrayLayer = 0,
                .layerCount = vk::RemainingArrayLayers,
            }
        };
        _view = info.device.createImageView(info_view);
    }
    void destroy(vk::Device device, vma::Allocator vmalloc) {
        vmalloc.destroyImage(_image, _allocation);
        device.destroyImageView(_view);
        _last_layout = vk::ImageLayout::eUndefined;
    }
    void transition_layout(TransitionInfo info) {
        vk::ImageMemoryBarrier2 image_barrier {
            .srcStageMask = info.src_stage,
            .srcAccessMask = info.src_access,
            .dstStageMask = info.dst_stage,
            .dstAccessMask = info.dst_access,
            .oldLayout = _last_layout,
            .newLayout = info.new_layout,
            .image = _image,
            .subresourceRange {
                .aspectMask = _aspects,
                .baseMipLevel = 0,
                .levelCount = vk::RemainingMipLevels,
                .baseArrayLayer = 0,
                .layerCount = vk::RemainingArrayLayers,
            }
        };
        vk::DependencyInfo info_dep {
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &image_barrier,
        };
        info.cmd.pipelineBarrier2(info_dep);
        _last_layout = info.new_layout;
    }
    
    vma::Allocation _allocation;
    vk::Image _image;
    vk::ImageView _view;
    vk::Extent3D _extent;
    vk::Format _format;
    vk::ImageAspectFlags _aspects;
    vk::ImageLayout _last_layout = vk::ImageLayout::eUndefined;
};