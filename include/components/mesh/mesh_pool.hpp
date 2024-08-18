#pragma once
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

struct MeshPool {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& usage_queues, uint32_t transfer_queue) {
        _vmalloc = vmalloc;
        _usage_queues = usage_queues;
        _transfer_queue = transfer_queue;
    }
    void destroy() {

    }

    vma::Allocator _vmalloc;
    std::span<const uint32_t> _usage_queues;
    uint32_t _transfer_queue;
};

// todo: move this into mesh.hpp and replace Mesh
struct Mesh2 {
    enum class ePrimitive {
        eQuad,
        eCube,
        eSphere,
        eCylinder,
        eCone,
        eTorus
    };
    struct Pool {
        struct MetaData { // could use uint16_t in some places
            uint32_t _index_byte_offset;
            uint32_t _index_count;
            // combined vertex/index buffer
            vk::DeviceAddress _mesh_buffer_addr;
        };
        struct MeshData {
            vk::Buffer _mesh_buffer;
            vma::Allocation _mesh_allocation;
            MetaData _mesh_meta_data;
        };
        void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& usage_queues, uint32_t transfer_queue) {
            _vmalloc = vmalloc;
            _usage_queues = usage_queues;
            _transfer_queue = transfer_queue;
        }
        void destroy(vma::Allocator vmalloc) {
            // todo
        }
        static Pool& get() {
            static Pool pool;
            return pool;
        }
        
        auto allocate(std::string_view path) -> MeshData& {
            auto [iter, emplaced] = _mesh_map.emplace(path);
            // TODO: load mesh if newly emplaced
            // else return existing mesh data and increment instance count
        }
        auto allocate(ePrimitive primitive) -> MeshData& {
            // TODO: create preset primitives
        }

        vma::Allocator _vmalloc;
        std::span<const uint32_t> _usage_queues;
        uint32_t _transfer_queue;
        std::map<std::string_view, MeshData> _mesh_map; 
    };
    void init(ePrimitive primitive) {
    }
    void init(std::string_view path) {
    }
    void destroy() {
        // todo
    }

    Pool::MeshData* _mesh_data_p;
};