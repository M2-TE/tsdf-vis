#pragma once
#include "components/vertices.hpp"
#include "components/indices.hpp"

template<typename Vertex, typename Index = uint16_t> 
struct Mesh {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues, std::span<Vertex> data_vertices, std::span<Index> data_indices) {
        uint32_t i_queue = queues.front();
        _vertices.init(vmalloc, i_queue, data_vertices);
        _indices.init(vmalloc, i_queue, data_indices);
    }
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues, std::span<Vertex> data_vertices) {
        uint32_t i_queue = queues.front();
        _vertices.init(vmalloc, i_queue, data_vertices);
    }
    void destroy(vma::Allocator vmalloc) {
        _vertices.destroy(vmalloc);
        if (_indices._count > 0) _indices.destroy(vmalloc);
    }

    Vertices<Vertex> _vertices;
    Indices<Index> _indices;
};