#pragma once
#include "components/camera.hpp"
#include "extra/grid.hpp"
#include "extra/plymesh.hpp"

struct Scene {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        _camera.init(vmalloc, queues);
        _grid.init(vmalloc, queues, "data/hsfd.grid");
        _ply.init(vmalloc, queues, "data/hsfd.ply");
    }
    void destroy(vma::Allocator vmalloc) {
        _camera.destroy(vmalloc);
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
    Grid _grid;
    Plymesh _ply;
    // TODO: mesh pools
};