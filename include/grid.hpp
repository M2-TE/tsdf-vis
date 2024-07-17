#pragma once
#include <fstream>
//
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <fmt/core.h>
#include <glm/glm.hpp>

struct Grid {
    void init(vma::Allocator vmalloc, uint32_t i_queue) {
        std::ifstream file("../fastgrid.grid", std::ifstream::in);
        if (file.good()) {
            std::size_t n_points = 0;
            std::size_t n_cells = 0;
            float voxelsize = 0;
            
            // read header
            file >> n_points;
            fmt::println("{} points, {} cells, {} voxelsize", n_points, n_cells, voxelsize);
            
            // alloc and read points
            _points.reserve(1000);
            for (std::size_t i = 0; i < _points.capacity(); i++) {
                glm::vec4 point;
                file >> point.x >> point.y >> point.z >> point.w;
                _points.push_back(point);
                // fmt::println("{}, {}, {}\t signed distance: {}", point.x, point.y, point.z, point.w);
            }
            file.close();
        }
        else {
            fmt::println("unable to read grid: {}", "../fastgrid.grid");
            return;
        }
        
        // create vertex buffer
		vk::BufferCreateInfo info_buffer {
			.size = sizeof(glm::vec4) * _points.size(),	
			.usage = vk::BufferUsageFlagBits::eVertexBuffer,
			.sharingMode = vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = 1,
			.pQueueFamilyIndices = &i_queue,
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
		void* p_mapped_data = vmalloc.mapMemory(_allocation);
        
		// check for host coherency and visibility
		vk::MemoryPropertyFlags props = vmalloc.getAllocationMemoryProperties(_allocation);
		if (props & vk::MemoryPropertyFlagBits::eHostVisible) _require_staging = false;
		else _require_staging = true;
		if (props & vk::MemoryPropertyFlagBits::eHostCoherent) _require_flushing = false;
		else _require_flushing = true;
        
		// upload data
		if (_require_staging) fmt::println("ReBAR is recommended and not present");
		std::memcpy(p_mapped_data, _points.data(), sizeof(glm::vec4) * _points.size());
		if (_require_flushing) vmalloc.flushAllocation(_allocation, 0, sizeof(glm::vec4) * _points.size());
        vmalloc.unmapMemory(_allocation);
    }
    void destroy(vma::Allocator vmalloc) {
		vmalloc.destroyBuffer(_buffer, _allocation);
    }
    
	// cpu related
    std::vector<glm::vec4> _points;
    // gpu related
    vk::Buffer _buffer;
	vma::Allocation _allocation;
	bool _require_staging;
	bool _require_flushing;
};