#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <fmt/core.h>
#include "core/input.hpp"

struct Camera {
	struct BufferData {
		glm::mat4x4 matrix;
	};
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues, vk::Extent2D extent) {
        // create camera matrix buffer
		vk::BufferCreateInfo info_buffer {
			.size = sizeof(BufferData),	
			.usage = vk::BufferUsageFlagBits::eUniformBuffer,
			.sharingMode = vk::SharingMode::eExclusive,
			.queueFamilyIndexCount = queues.size(),
			.pQueueFamilyIndices = queues.data(),
		};
		vma::AllocationCreateInfo info_allocation {
			.flags = 
				vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
				vma::AllocationCreateFlagBits::eMapped,
			.usage = vma::MemoryUsage::eAutoPreferDevice,
			.requiredFlags = 
				vk::MemoryPropertyFlagBits::eDeviceLocal,
			.preferredFlags = 
				vk::MemoryPropertyFlagBits::eHostCoherent |
				vk::MemoryPropertyFlagBits::eHostVisible, // ReBAR
		};
		std::tie(_buffer, _allocation) = vmalloc.createBuffer(info_buffer, info_allocation);
		_mapped_data_p = vmalloc.mapMemory(_allocation);

		// check for host coherency and visibility
		vk::MemoryPropertyFlags props = vmalloc.getAllocationMemoryProperties(_allocation);
		if (props & vk::MemoryPropertyFlagBits::eHostVisible) _require_staging = false;
		else _require_staging = true;
		if (props & vk::MemoryPropertyFlagBits::eHostCoherent) _require_flushing = false;
		else _require_flushing = true;
		
		// initialize camera size
		resize(extent);
    }
    void destroy(vma::Allocator vmalloc) {
		vmalloc.unmapMemory(_allocation);
		vmalloc.destroyBuffer(_buffer, _allocation);
    }
    
    void resize(vk::Extent2D extent) {
		_extent = extent;
    }
	void update(vma::Allocator vmalloc) {
		// read input for movement and rotation
		float speed = 0.05;
		if (Keys::down(SDLK_LCTRL)) speed /= 8.0;
		if (Keys::down(SDLK_LSHIFT)) speed *= 8.0;

		// move in direction relative to camera
		glm::quat q_rot(_rot);
		if (Keys::down('w')) _pos += q_rot * glm::vec3(0, 0, +speed);
		if (Keys::down('s')) _pos += q_rot * glm::vec3(0, 0, -speed);
		if (Keys::down('d')) _pos += q_rot * glm::vec3(+speed, 0, 0);
		if (Keys::down('a')) _pos += q_rot * glm::vec3(-speed, 0, 0);
		if (Keys::down('q')) _pos += q_rot * glm::vec3(0, +speed, 0);
		if (Keys::down('e')) _pos += q_rot * glm::vec3(0, -speed, 0);

		// only control camera when mouse is captured
		if (SDL_GetRelativeMouseMode()) {
			_rot += glm::vec3(-Mouse::delta().second, +Mouse::delta().first, 0) * 0.005f;
		}

		// merge rotation and projection matrices
		BufferData data;
		data.matrix = glm::perspectiveFovLH<float>(glm::radians<float>(_fov), _extent.width, _extent.height, _near, _far);
		data.matrix = data.matrix * glm::eulerAngleXY(-_rot.x, -_rot.y);
		data.matrix = glm::translate(data.matrix, -_pos);
		
		// upload data
		if (_require_staging) fmt::println("ReBAR is required and not present");
		std::memcpy(_mapped_data_p, &data, sizeof(BufferData));
		if (_require_flushing) vmalloc.flushAllocation(_allocation, 0, sizeof(BufferData));
	}
    
	// cpu related
	vk::Extent2D _extent;
	glm::vec3 _pos = { 0, 0, 0 };
	glm::vec3 _rot = { 0, 0, 0 };
	float _fov = 75;
	float _near = 0.01;
	float _far = 50.0;
    // gpu related
    vk::Buffer _buffer;
	vma::Allocation _allocation;
	void* _mapped_data_p;
	bool _require_staging;
	bool _require_flushing;
};