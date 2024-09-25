#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_aligned.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <fmt/base.h>
#include "core/input.hpp"
#include "core/buffer.hpp"

struct Camera {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        // create camera matrix buffer
		_buffer.init(vmalloc,
			vk::BufferCreateInfo {
				.size = _buffer.size(),	
				.usage = vk::BufferUsageFlagBits::eUniformBuffer,
				.sharingMode = vk::SharingMode::eExclusive,
				.queueFamilyIndexCount = queues.size(),
				.pQueueFamilyIndices = queues.data(),
			},
			vma::AllocationCreateInfo {
				.flags = 
					vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
					vma::AllocationCreateFlagBits::eMapped,
				.usage = vma::MemoryUsage::eAutoPreferDevice,
				.requiredFlags = 
					vk::MemoryPropertyFlagBits::eDeviceLocal,
				.preferredFlags = 
					vk::MemoryPropertyFlagBits::eHostCoherent |
					vk::MemoryPropertyFlagBits::eHostVisible, // ReBAR
			}
		);
    }
    void destroy(vma::Allocator vmalloc) {
		_buffer.destroy(vmalloc);
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
		glm::qua<float, glm::aligned_highp> q_rot(_rot);
		if (Keys::down('w')) _pos += q_rot * glm::aligned_vec3(0, 0, +speed);
		if (Keys::down('s')) _pos += q_rot * glm::aligned_vec3(0, 0, -speed);
		if (Keys::down('d')) _pos += q_rot * glm::aligned_vec3(+speed, 0, 0);
		if (Keys::down('a')) _pos += q_rot * glm::aligned_vec3(-speed, 0, 0);
		if (Keys::down('q')) _pos += q_rot * glm::aligned_vec3(0, +speed, 0);
		if (Keys::down('e')) _pos += q_rot * glm::aligned_vec3(0, -speed, 0);

		// only control camera when mouse is captured
		if (Mouse::captured()) {
			_rot += glm::aligned_vec3(-Mouse::delta().second, +Mouse::delta().first, 0) * 0.005f;
		}

		// merge rotation and projection matrices
		glm::aligned_mat4x4 matrix;
		matrix = glm::perspectiveFovLH<float>(glm::radians<float>(_fov), (float)_extent.width, (float)_extent.height, _near, _far);
		matrix = glm::rotate(matrix, -_rot.x, glm::aligned_vec3(1, 0, 0));
		matrix = glm::rotate(matrix, -_rot.y, glm::aligned_vec3(0, 1, 0));
		matrix = glm::translate(matrix, -_pos);
		
		// upload data
		_buffer.write(vmalloc, matrix);
	}

	glm::aligned_vec3 _pos = { 0, 0, 0 };
	glm::aligned_vec3 _rot = { 0, 0, 0 };
	DeviceBuffer<glm::aligned_mat4x4> _buffer;
	vk::Extent2D _extent;
	float _fov = 60;
	float _near = 0.01;
	float _far = 1000.0;
};