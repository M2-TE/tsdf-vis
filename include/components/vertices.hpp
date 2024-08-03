#pragma once
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>

template<typename Vertex>
struct Vertices {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues, std::span<Vertex> vertex_data) {
        // create vertex buffer
		vk::BufferCreateInfo info_buffer {
			.size = sizeof(Vertex) * vertex_data.size(),	
			.usage = vk::BufferUsageFlagBits::eVertexBuffer,
			.sharingMode = vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = queues.size(),
			.pQueueFamilyIndices = queues.data(),
		};
		vma::AllocationCreateInfo info_allocation {
			.flags = 
				vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
				vma::AllocationCreateFlagBits::eDedicatedMemory,
			.usage = 
                vma::MemoryUsage::eAutoPreferDevice,
			.requiredFlags = 
				vk::MemoryPropertyFlagBits::eDeviceLocal,
			.preferredFlags = 
				vk::MemoryPropertyFlagBits::eHostCoherent |
				vk::MemoryPropertyFlagBits::eHostVisible // ReBAR
		};
		std::tie(_buffer, _allocation) = vmalloc.createBuffer(info_buffer, info_allocation);
        
		// check for host coherency and visibility
		bool _require_staging;
		bool _require_flushing;
		vk::MemoryPropertyFlags props = vmalloc.getAllocationMemoryProperties(_allocation);
		if (props & vk::MemoryPropertyFlagBits::eHostCoherent) _require_flushing = false;
		else _require_flushing = true;
		if (props & vk::MemoryPropertyFlagBits::eHostVisible) _require_staging = false;
		else _require_staging = true;
        
		// upload data
		if (_require_staging) fmt::println("ReBAR is recommended but not present");
		void* mapped_data_p = vmalloc.mapMemory(_allocation);
		std::memcpy(mapped_data_p, vertex_data.data(), sizeof(Vertex) * vertex_data.size());
		if (_require_flushing) vmalloc.flushAllocation(_allocation, 0, sizeof(Vertex) * vertex_data.size());
        vmalloc.unmapMemory(_allocation);
        _vertex_n = vertex_data.size();
    }
    void destroy(vma::Allocator vmalloc) {
		vmalloc.destroyBuffer(_buffer, _allocation);
    }

    uint32_t _vertex_n = 0;
    vk::Buffer _buffer;
	vma::Allocation _allocation;
};