#pragma once
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>

struct MeshPool {
    void init(vma::Allocator vmalloc) {
        vma::VirtualAllocation virt_alloc;
        // todo
    }
    void destroy(vma::Allocator vmalloc) {

    }

    vma::Allocation _alloc;
    vk::Buffer _mesh_buffer;
};