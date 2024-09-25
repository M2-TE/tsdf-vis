#pragma once
// #include <random>
#include <fmt/format.h>
#include "components/transform/camera.hpp"
#include "components/extra/grid.hpp"
#include "components/extra/plymesh.hpp"

struct Scene {
    struct SceneData {
        Grid _grid;
        Plymesh _mesh_main;
        Plymesh _mesh_main_grey;
        std::vector<Plymesh> _mesh_subs;
    };
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues) {
        _camera.init(vmalloc, queues);
        
        _data._grid.init(vmalloc, queues, "data/hsfd23/hashgrid.grid");
        _data._mesh_main.init(vmalloc, queues, "data/hsfd23/mesh.ply");
        _data._mesh_main_grey.init(vmalloc, queues, "data/hsfd23/mesh.ply", glm::vec3(0.5, 0.5, 0.5));

        // std::random_device rd;
        // std::mt19937 gen(rd());
        // std::uniform_real_distribution<float> dis(0.0, 1.0);
        static constexpr std::size_t subs_n = 30;
        _data._mesh_subs.resize(subs_n);
        for (size_t i = 0; i < subs_n; i++) {
            // glm::vec3 color = { dis(gen), dis(gen), dis(gen) };
            glm::vec3 color = { 1.0, 0.1, 0.1 };
            _data._mesh_subs[i].init(vmalloc, queues, std::format("data/hsfd23/mesh_{}.ply", i), color);
        }
    }
    void destroy(vma::Allocator vmalloc) {
        _camera.destroy(vmalloc);
        _data._grid.destroy(vmalloc);
        _data._mesh_main.destroy(vmalloc);
        _data._mesh_main_grey.destroy(vmalloc);
        for (auto& mesh: _data._mesh_subs) {
            mesh.destroy(vmalloc);
        }
    }

    void static SDLCALL folder_callback(void* userdata_p, const char* const* filelist_pp, int /*filter*/) {
        if (!filelist_pp) {
            fmt::println("An error occured: {}", SDL_GetError());
            return;
        } 
        else
        if (!*filelist_pp) {
            fmt::println("No folder selected.");
            return;
        }
        fmt::println("Full path to selected file: {}", *filelist_pp);

        Scene* scene_p = static_cast<Scene*>(userdata_p);
        scene_p->_camera.resize(scene_p->_camera._extent);
    }

    // update without affecting current frames in flight
    void update_safe() {
        if (Keys::down(SDLK_LCTRL) && Keys::pressed('o')) {
            SDL_ShowOpenFolderDialog(folder_callback, this, nullptr, SDL_GetBasePath(), false);
        }

        // go to next subtree
        if (Keys::pressed(SDLK_RIGHT)) {
            _mesh_sub_i = (_mesh_sub_i + 1) % (uint32_t)_data._mesh_subs.size();
        }
        // go to previous subtree
        if (Keys::pressed(SDLK_LEFT)) {
            if (_mesh_sub_i == 0) _mesh_sub_i = (uint32_t)_data._mesh_subs.size();
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
    SceneData _data;
    uint32_t _mesh_sub_i = 0;
    // toggle flags
    bool _render_grid = false;
    bool _render_grey = false;
    bool _render_subs = false;
};