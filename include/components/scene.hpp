#pragma once
#include "components/grid.hpp"
#include "components/plymesh.hpp"

struct Scene {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        _grid.init(vmalloc, queues, "sphere.grid");
        _ply.init(vmalloc, queues, "sphere.ply");
    }
    void destroy(vma::Allocator vmalloc) {
        _grid.destroy(vmalloc);
        _ply.destroy(vmalloc);
    }

    Grid _grid;
    Plymesh _ply;
    // TODO: mesh pools
};