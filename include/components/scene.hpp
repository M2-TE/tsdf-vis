#pragma once
#include "extra/grid.hpp"
#include "extra/plymesh.hpp"

struct Scene {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        _grid.init(vmalloc, queues, "data/hsfd.grid");
        _ply.init(vmalloc, queues, "data/hsfd.ply");
    }
    void destroy(vma::Allocator vmalloc) {
        _grid.destroy(vmalloc);
        _ply.destroy(vmalloc);
    }

    Grid _grid;
    Plymesh _ply;
    // TODO: mesh pools
};