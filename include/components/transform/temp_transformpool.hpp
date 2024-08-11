#pragma once
#include <cstdint>
#include <queue>
#include <set>
//
#include <vk_mem_alloc.hpp>

struct TransformPool {
	void init(vk::Device device, vma::Allocator vmalloc, uint32_t iQueue) {
		this->vmalloc = vmalloc;
		
		// create single large memory block
		constexpr vk::DeviceSize nBytes = 1ull << 28ull; // 256 MiB
		vk::BufferCreateInfo buffInfo {
			.size = nBytes,	
			.usage = vk::BufferUsageFlagBits::eShaderDeviceAddress,
			.sharingMode = vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = 1,
			.pQueueFamilyIndices = &iQueue,
		};
		vma::AllocationCreateInfo createInfo {
			.flags = 
				vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
				vma::AllocationCreateFlagBits::eDedicatedMemory,
			.usage = vma::MemoryUsage::eAutoPreferDevice,
			.requiredFlags = 
				vk::MemoryPropertyFlagBits::eDeviceLocal,
			.preferredFlags = 
				vk::MemoryPropertyFlagBits::eHostCoherent |
				vk::MemoryPropertyFlagBits::eHostVisible // ReBAR
		};
		std::tie(buffer, allocation) = vmalloc.createBufferWithAlignment(buffInfo, createInfo, sizeof(TransformData));
		pMappedData = vmalloc.mapMemory(allocation);

		// check for host coherency and visibility
		vk::MemoryPropertyFlags props = vmalloc.getAllocationMemoryProperties(allocation);
		if (props & vk::MemoryPropertyFlagBits::eHostVisible) bRequireStaging = false;
		else bRequireStaging = true;
		if (props & vk::MemoryPropertyFlagBits::eHostCoherent) bRequireFlushing = false;
		else bRequireFlushing = true;

		// device buffer address
		vk::BufferDeviceAddressInfo addrInfo { .buffer = buffer };
		bufferAddress = device.getBufferAddress(addrInfo);
	}
	void destroy() {
		vmalloc.unmapMemory(allocation);
		vmalloc.destroyBuffer(buffer, allocation);
	}
	
	uint32_t allocate() {
		if (freedAllocations.size() > 0) {
			uint32_t index = freedAllocations.front();
			freedAllocations.pop();
			return index;
		}
		else return nAllocatedTransforms++;
	}
	void deallocate(uint32_t index) {
		freedAllocations.push(index);
	}
	void update_buffer(glm::mat3x3& mat, glm::vec3& pos, glm::vec3& err, uint32_t index) {
		TransformData data = { mat, pos, err };
		if (bRequireStaging) fmt::println("ReBAR is required");
		std::memcpy((char*)pMappedData + index * sizeof(TransformData), &data, sizeof(TransformData));
		if (bRequireFlushing) fmt::println("TODO: mem flushing");
	}

	// configuration
	vma::Allocator vmalloc;
	vma::Allocation allocation;
	vk::Buffer buffer;
	vk::DeviceAddress bufferAddress;
	void* pMappedData;
	bool bRequireStaging;
	bool bRequireFlushing;
	// allocation
	struct alignas(64) TransformData {
		glm::mat3x3 modelMatrix;
		glm::vec3 position;
		glm::vec3 positionError;
		float padding;
	};
	uint32_t nAllocatedTransforms = 1; // reserve first index
	std::queue<uint32_t> freedAllocations;
};