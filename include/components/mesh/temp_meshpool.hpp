#pragma once
#include <cstdint>
#include <set>
//
#include <vk_mem_alloc.hpp>

struct MeshPool {
	void init(vma::Allocator vmalloc, uint32_t iQueue) {
		this->vmalloc = vmalloc;
		this->iQueue = iQueue;
	}
	void destroy() {
		for (auto& block: meshBlocks) {
			block.virtualBlock.clearVirtualBlock(); // lazy workaround..
			block.virtualBlock.destroy();
			vmalloc.unmapMemory(block.allocation);
			vmalloc.destroyBuffer(block.buffer, block.allocation);
		}
		meshBlocks.clear();
	}
	
	template<typename Vertex, typename Index>
	std::pair<uint32_t, uint32_t> allocate(std::vector<Vertex>& vertices, std::vector<Index>& indices, uint32_t id) {
		// select block
		uint32_t blockIndex = 0;
		if (meshBlocks.size() == blockIndex) allocate_block();
		auto& block = meshBlocks[blockIndex];

		// calculate sizes
		vk::DeviceSize nVertexBytes = vertices.size() * sizeof(Vertex);
		vk::DeviceSize nIndexBytes = indices.size() * sizeof(Index);
		vk::DeviceSize offset;

		// suballocate for entire mesh
		vma::VirtualAllocationCreateInfo virtAllocInfo(nVertexBytes + nIndexBytes);
		auto virtAlloc = block.virtualBlock.virtualAllocate(virtAllocInfo, offset); // todo: handle failure

		// store virtual allocation
		block.meshAllocs.push_back(virtAlloc);
		// store mesh metadata
		auto& mesh = block.meshes.emplace_back();
		mesh.vertexByteOffset = offset;
		mesh.indexCountOffset = (offset + nVertexBytes) / sizeof(Index);
		mesh.nIndices = indices.size();
		mesh.id = id;

		// copy data into buffers
		if (block.bRequireStaging) fmt::println("ReBAR is required");
		std::memcpy((char*)block.pMappedData + offset, vertices.data(), nVertexBytes);
		std::memcpy((char*)block.pMappedData + offset + nVertexBytes, indices.data(), nIndexBytes);
		if (block.bRequireFlushing) fmt::println("TODO: mem flushing");

		// return block and block-local mesh index
		return { blockIndex, (uint32_t)block.meshes.size() - 1 };
	}
	void deallocate(uint32_t iBlock, uint32_t iAlloc) {
		//auto& block = meshBlocks[iBlock];
		//block.virtualBlock.virtualFree(block.meshAllocs[iAlloc]);
		//block.meshes.
	}
	void allocate_block() {
		auto& block = meshBlocks.emplace_back();
		constexpr vk::DeviceSize nBytes = 1ull << 28ull; // 256 MiB

		// create single large memory block
		vk::BufferCreateInfo buffInfo = {
			.size = nBytes,
			.usage =
				vk::BufferUsageFlagBits::eVertexBuffer |
				vk::BufferUsageFlagBits::eIndexBuffer,
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
		std::tie(block.buffer, block.allocation) = vmalloc.createBuffer(buffInfo, createInfo);
		block.pMappedData = vmalloc.mapMemory(block.allocation);

		// check for host coherency and visibility
		vk::MemoryPropertyFlags props = vmalloc.getAllocationMemoryProperties(block.allocation);
		if (props & vk::MemoryPropertyFlagBits::eHostVisible) block.bRequireStaging = false;
		else block.bRequireStaging = true;
		if (props & vk::MemoryPropertyFlagBits::eHostCoherent) block.bRequireFlushing = false;
		else block.bRequireFlushing = true;

		// create virtual block to suballocate buffer for different meshes
		vma::VirtualBlockCreateInfo virtBlockInfo(nBytes);
		block.virtualBlock = vma::createVirtualBlock(virtBlockInfo);
	}
	
	// configuration
	vma::Allocator vmalloc;
	uint32_t iQueue;
	// allocation
	struct MeshMetadata {
		uint32_t vertexByteOffset;
		uint32_t indexCountOffset;
		uint32_t nIndices;
		uint32_t id;
	};
	struct MemoryBlock {
		std::vector<MeshMetadata> meshes;
		std::vector<vma::VirtualAllocation> meshAllocs;
		vk::Buffer buffer;
		vma::Allocation allocation;
		vma::VirtualBlock virtualBlock;
		void* pMappedData;
		bool bRequireStaging;
		bool bRequireFlushing;
		bool bFull = false; // todo: maybe a free space variable instead?
	};
	std::vector<MemoryBlock> meshBlocks;
};