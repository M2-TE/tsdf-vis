#pragma once
// #include <random>
#include <fmt/format.h>
#include "components/transform/camera.hpp"
#include "extra/grid.hpp"
#include "extra/plymesh.hpp"

struct Scene {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        _camera.init(vmalloc, queues);
        _grid.init(vmalloc, queues, "data/hsfd23/hashgrid.grid");
        _mesh_main.init(vmalloc, queues, "data/hsfd23/mesh.ply");
        _mesh_main_grey.init(vmalloc, queues, "data/hsfd23/mesh.ply", glm::vec3(0.5, 0.5, 0.5));

        // std::random_device rd;
        // std::mt19937 gen(rd());
        // std::uniform_real_distribution<float> dis(0.0, 1.0);
        for (size_t i = 0; i < 30; i++) {
            _mesh_subs.emplace_back();
            // glm::vec3 color = { dis(gen), dis(gen), dis(gen) };
            glm::vec3 color = { 1.0, 0.1, 0.1 };
            _mesh_subs.back().init(vmalloc, queues, std::format("data/hsfd23/mesh_{}.ply", i), color);
        }
    }
    void destroy(vma::Allocator vmalloc) {
        _camera.destroy(vmalloc);
        _grid.destroy(vmalloc);
        _mesh_main.destroy(vmalloc);
        _mesh_main_grey.destroy(vmalloc);
        for (auto& mesh: _mesh_subs) {
            mesh.destroy(vmalloc);
        }
    }

    // update without affecting current frames in flight
    void update_safe(vma::Allocator vmalloc) {
        // ImGui::Begin("subtree index");
        // ImGui::Text(fmt::format("subtree index: {}", _mesh_sub_i).c_str());
        // ImGui::End();
        // ImGui::Begin("camera pose");
        // ImGui::Text(fmt::format("position: {:.2f} {:.2f} {:.2f}", _camera._pos.x, _camera._pos.y, _camera._pos.z).c_str());
        // ImGui::Text(fmt::format("rotation: {:.2f} {:.2f} {:.2f}", _camera._rot.x, _camera._rot.y, _camera._rot.z).c_str());
        // ImGui::End();

        // go to next subtree
        if (Keys::pressed(SDLK_RIGHT)) {
            _mesh_sub_i = (_mesh_sub_i + 1) % _mesh_subs.size();
        }
        // go to previous subtree
        if (Keys::pressed(SDLK_LEFT)) {
            if (_mesh_sub_i == 0) _mesh_sub_i = _mesh_subs.size();
            _mesh_sub_i = _mesh_sub_i - 1;
        }

        if (Keys::pressed(SDLK_UP)) {
            _render_grey = !_render_grey;
        }
        if (Keys::pressed(SDLK_DOWN)) {
            _render_subs = !_render_subs;
        }
        if (Keys::pressed(SDLK_SPACE)) {
            _render_grid = !_render_grid;
        }
    }
    // update after buffers are no longer being read
    void update(vma::Allocator vmalloc) {
        _camera.update(vmalloc);
    }

    Camera _camera;
    Grid _grid;
    Plymesh _mesh_main;
    Plymesh _mesh_main_grey;
    uint32_t _mesh_sub_i = 0;
    std::vector<Plymesh> _mesh_subs;
    // toggle flags
    bool _render_grid = false;
    bool _render_grey = false;
    bool _render_subs = false;
};