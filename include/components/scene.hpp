#pragma once
#include <random>
#include <fmt/format.h>
#include "components/transform/camera.hpp"
#include "extra/grid.hpp"
#include "extra/plymesh.hpp"

struct Scene {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        _camera.init(vmalloc, queues);
        _grid.init(vmalloc, queues, "data/mesh.grid");
        _mesh_main.init(vmalloc, queues, "data/mesh.ply");

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(0.0, 1.0);
        for (size_t i = 0; i < 5; i++) {
            _mesh_subs.emplace_back();
            glm::vec3 color = { dis(gen), dis(gen), dis(gen) };
            _mesh_subs.back().init(vmalloc, queues, std::format("data/mesh_{}.ply", i), color);
        }
    }
    void destroy(vma::Allocator vmalloc) {
        _camera.destroy(vmalloc);
        _grid.destroy(vmalloc);
        _mesh_main.destroy(vmalloc);
        for (auto& mesh: _mesh_subs) {
            mesh.destroy(vmalloc);
        }
    }

    // update without affecting current frames in flight
    void update_safe(vma::Allocator vmalloc) {
        ImGui::Begin("subtree index");
        ImGui::Text("subtree index :%d", _mesh_sub_i);
        ImGui::End();

        // go to next subtree
        if (Keys::pressed(SDLK_RIGHT)) {
            _mesh_sub_i = (_mesh_sub_i + 1) % _mesh_subs.size();
        }
    }
    // update after buffers are no longer being read
    void update(vma::Allocator vmalloc) {
        _camera.update(vmalloc);
    }

    Camera _camera;
    Grid _grid;
    Plymesh _mesh_main;
    uint32_t _mesh_sub_i = 0;
    std::vector<Plymesh> _mesh_subs;
};