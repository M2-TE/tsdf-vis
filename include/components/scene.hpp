#pragma once
#include "components/transform/camera.hpp"
// #include "components/mesh/mesh_pool.hpp"
#include "extra/grid.hpp"
#include "extra/plymesh.hpp"

struct Scene {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        _camera.init(vmalloc, queues);
        // _mesh_pool.init(vmalloc, queues);
        _grid.init(vmalloc, queues, "data/hsfd.grid");
        _ply.init(vmalloc, queues, "data/hsfd.ply");
    }
    void destroy(vma::Allocator vmalloc) {
        _camera.destroy(vmalloc);
        // _mesh_pool.destroy(vmalloc);
        _grid.destroy(vmalloc);
        _ply.destroy(vmalloc);
    }

    // update without affecting current frames in flight
    void update_safe(vma::Allocator vmalloc) {
    }
    // update after buffers are no longer being read
    void update(vma::Allocator vmalloc) {
        _camera.update(vmalloc);
    }

    Camera _camera;
    // MeshPool _mesh_pool;
    Grid _grid;
    Plymesh _ply;
};