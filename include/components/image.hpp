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
    struct WrapInfo {
        vk::Image image;
        vk::ImageView image_view;
        vk::Extent3D extent;
        vk::ImageAspectFlags aspects;
    };
    struct TransitionInfo {
        vk::CommandBuffer cmd;
        vk::ImageLayout new_layout;
        vk::PipelineStageFlags2 dst_stage = vk::PipelineStageFlagBits2::eAllCommands;
        vk::AccessFlags2 dst_access = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
    };
    
    void init(CreateInfo& info) {
        _owning = true;
        _format = info.format;
        _extent = info.extent;
        _aspects = info.aspects;
        _last_layout = vk::ImageLayout::eUndefined;
        _last_access = vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite;
        _last_stage = vk::PipelineStageFlagBits2::eAllCommands;
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
            .components {
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity,
            },
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
    void wrap(WrapInfo& info) {
        _owning = false;
        _image = info.image;
        _view = info.image_view;
        _extent = info.extent;
        _aspects = info.aspects;
        _last_layout = vk::ImageLayout::eUndefined;
    }
    void destroy(vk::Device device, vma::Allocator vmalloc) {
        if (_owning) {
            vmalloc.destroyImage(_image, _allocation);
            device.destroyImageView(_view);
        }
    }
    
    void load_texture(vma::Allocator vmalloc, std::span<const std::byte> tex_data) {
        // create image data buffer
		vk::BufferCreateInfo info_buffer {
			.size = tex_data.size(),
			.usage = vk::BufferUsageFlagBits::eVertexBuffer,
			.sharingMode = vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
		};
		vma::AllocationCreateInfo info_allocation {
			.flags =
				vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
				vma::AllocationCreateFlagBits::eDedicatedMemory,
			.usage = 
                vma::MemoryUsage::eAuto,
			.requiredFlags =
				vk::MemoryPropertyFlagBits::eDeviceLocal,
			.preferredFlags =
				vk::MemoryPropertyFlagBits::eHostCoherent |
				vk::MemoryPropertyFlagBits::eHostVisible // ReBAR
		};
		auto [staging_buffer, staging_alloc] = vmalloc.createBuffer(info_buffer, info_allocation);

        // upload data
        void* mapped_data_p = vmalloc.mapMemory(staging_alloc);
        std::memcpy(mapped_data_p, tex_data.data(), tex_data.size());
        vmalloc.unmapMemory(staging_alloc);
        
        // clean up staging buffer
        vmalloc.destroyBuffer(staging_buffer, staging_alloc);
    }
    void transition_layout(TransitionInfo& info) {
        vk::ImageMemoryBarrier2 image_barrier {
            .srcStageMask = _last_stage,
            .srcAccessMask = _last_access,
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
        _last_access = info.dst_access;
        _last_stage = info.dst_stage;
    }
    void blit(vk::CommandBuffer cmd, Image& src_image) {
        vk::ImageBlit2 region {
            .srcSubresource { 
                .aspectMask = src_image._aspects,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets = std::array<vk::Offset3D, 2>{ 
                vk::Offset3D(), 
                vk::Offset3D(src_image._extent.width, src_image._extent.height, 1) },
            .dstSubresource { 
                .aspectMask = _aspects,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffsets = std::array<vk::Offset3D, 2>{ 
                vk::Offset3D(), 
                vk::Offset3D(_extent.width, _extent.height, 1) },
        };
        vk::BlitImageInfo2 info_blit {
            .srcImage = src_image._image,
            .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
            .dstImage = _image,
            .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
            .regionCount = 1,
            .pRegions = &region,
            .filter = vk::Filter::eLinear,
        };
        cmd.blitImage2(info_blit);
    }
    
    vma::Allocation _allocation;
    vk::Image _image;
    vk::ImageView _view;
    vk::Extent3D _extent;
    vk::Format _format;
    vk::ImageAspectFlags _aspects;
    vk::ImageLayout _last_layout;
    vk::AccessFlags2 _last_access;
    vk::PipelineStageFlags2 _last_stage;
    bool _owning;
};