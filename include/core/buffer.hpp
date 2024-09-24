#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>

template<typename T>
struct DeviceBuffer {
	void init(vma::Allocator vmalloc, const vk::BufferCreateInfo& info_buffer, const vma::AllocationCreateInfo& info_allocation) {
		std::tie(_data, _allocation) = vmalloc.createBuffer(info_buffer, info_allocation);
		_mapped_data_p = vmalloc.mapMemory(_allocation);

		// check for host coherency and visibility
		vk::MemoryPropertyFlags props = vmalloc.getAllocationMemoryProperties(_allocation);
		if (props & vk::MemoryPropertyFlagBits::eHostVisible) _require_staging = false;
		else _require_staging = true;
		if (props & vk::MemoryPropertyFlagBits::eHostCoherent) _require_flushing = false;
		else _require_flushing = true;
	}
	void destroy(vma::Allocator vmalloc) {
		vmalloc.unmapMemory(_allocation);
		vmalloc.destroyBuffer(_data, _allocation);
	}
	
	auto static constexpr size() -> vk::DeviceSize {
		return sizeof(T);
	}
	void write(vma::Allocator vmalloc, const T& data) {
		if (_require_staging) fmt::println("ReBAR is currently required and not present");
		std::memcpy(_mapped_data_p, &data, sizeof(T));
		if (_require_flushing) vmalloc.flushAllocation(_allocation, 0, sizeof(T));
	}
	void write_partial() {/*TODO*/}

	vk::Buffer _data;
	vma::Allocation _allocation;
	// todo: is there a cost to leaving resources mapped?
	// todo: no need to map when staging buffer is required anyways
	void* _mapped_data_p;
	bool _require_staging;
	bool _require_flushing;
};