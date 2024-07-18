#pragma once
#include <fstream>
//
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <fmt/core.h>
#include <glm/glm.hpp>

struct Grid {
    void init(vma::Allocator vmalloc, uint32_t i_queue) {
		std::ifstream file;
		std::string filepath = "../hashgrid.grid";
		file.open(filepath, std::ifstream::binary);
        if (file.good()) {
            float voxelsize = 0;
            std::size_t n_scan_points = 0;
            std::size_t n_query_points = 0;
            std::size_t n_cells = 0;
			
			file.read(reinterpret_cast<char*>(&voxelsize), sizeof(float));
			file.read(reinterpret_cast<char*>(&n_scan_points), sizeof(std::size_t));
			file.read(reinterpret_cast<char*>(&n_query_points), sizeof(std::size_t));
			file.read(reinterpret_cast<char*>(&n_cells), sizeof(std::size_t));
            
            // read header
            fmt::println("voxelsize of: {} with {} scan points, {} query points and {} cells", voxelsize, n_scan_points, n_query_points, n_cells);
            
            // alloc and read points
            // _points.reserve(1000);
            // for (std::size_t i = 0; i < _points.capacity(); i++) {
            //     glm::vec4 point;
            //     file >> point.x >> point.y >> point.z >> point.w;
            //     _points.push_back(point);
            //     // fmt::println("{}, {}, {}\t signed distance: {}", point.x, point.y, point.z, point.w);
            // }
            file.close();
        }
        else {
            fmt::println("unable to read grid: {}", filepath);
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