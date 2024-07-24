#pragma once
#include "components/grid.hpp"

struct Scene {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        _grid.init(vmalloc, queues);
    }
    void destroy(vma::Allocator vmalloc) {
        _grid.destroy(vmalloc);
    }

    Grid _grid;
    // TODO: mesh pools
};