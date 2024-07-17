#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>

struct Image {
    enum Access { R, W, RW };
    void init(vk::Device device, vma::Allocator vmalloc, 
        vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, 
        vk::ImageAspectFlags aspects = vk::ImageAspectFlagBits::eColor)
    {
        _extent = extent;
        _format = format;
        // create image
        vk::ImageCreateInfo info_image {
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
        };
        vma::AllocationCreateInfo info_alloc {
            .flags = vma::AllocationCreateFlagBits::eDedicatedMemory,
            .usage = vma::MemoryUsage::eAutoPreferDevice,
            .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
        };
        std::tie(_image, _allocation) = vmalloc.createImage(info_image, info_alloc);
        
        // create image view
        vk::ImageViewCreateInfo info_view {
            .image = _image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange {
                .aspectMask = aspects,
                .baseMipLevel = 0,
                .levelCount = vk::RemainingMipLevels,
                .baseArrayLayer = 0,
                .layerCount = vk::RemainingArrayLayers,
            }
        };
        _view = device.createImageView(info_view);
    }
    void init(vk::Device device, vma::Allocator vmalloc, 
        vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage, 
        vk::ImageAspectFlags aspects = vk::ImageAspectFlagBits::eColor)
    {
        vk::Extent3D extent_3d { .width = extent.width, .height = extent.height, .depth = 1 };
        init(device, vmalloc, extent_3d, format, usage, aspects);
    }
    void destroy(vk::Device device, vma::Allocator vmalloc)
    {
        vmalloc.destroyImage(_image, _allocation);
        device.destroyImageView(_view);
        _last_layout = vk::ImageLayout::eUndefined;
    }
    
    void transition_layout(vk::CommandBuffer cmd, vk::ImageLayout new_layout, 
        vk::PipelineStageFlags2 src_stage, vk::PipelineStageFlags2 dst_stage,
        vk::AccessFlags2 src_access, vk::AccessFlags2 dst_access)
    {
        vk::ImageMemoryBarrier2 image_barrier {
            .srcStageMask = src_stage,
            .srcAccessMask = src_access,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_access,
            .oldLayout = _last_layout,
            .newLayout = new_layout,
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
        cmd.pipelineBarrier2(info_dep);
        _last_layout = new_layout;
    }
    template<Access aSrc, Access aDst>
    void transition_layout(vk::CommandBuffer cmd, vk::ImageLayout new_layout, 
        vk::PipelineStageFlags2 src_stage = vk::PipelineStageFlagBits2::eAllCommands, 
        vk::PipelineStageFlags2 dst_stage = vk::PipelineStageFlagBits2::eAllCommands)
    {
        vk::AccessFlags2 src_flags, dst_flags;
        switch (aSrc) {
            case R: src_flags = vk::AccessFlagBits2::eMemoryRead; break;
            case W: src_flags = vk::AccessFlagBits2::eMemoryWrite; break;
            case RW: src_flags = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite; break;
        }
        switch (aDst) {
            case R: dst_flags = vk::AccessFlagBits2::eMemoryRead; break;
            case W: dst_flags = vk::AccessFlagBits2::eMemoryWrite; break;
            case RW: dst_flags = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite; break;
        }
        transition_layout(cmd, new_layout, src_stage, dst_stage, src_flags, dst_flags);
    }
    
    vma::Allocation _allocation;
    vk::Image _image;
    vk::ImageView _view;
    vk::Extent3D _extent;
    vk::Format _format;
    vk::ImageAspectFlags _aspects = vk::ImageAspectFlagBits::eColor;
    vk::ImageLayout _last_layout = vk::ImageLayout::eUndefined;
};