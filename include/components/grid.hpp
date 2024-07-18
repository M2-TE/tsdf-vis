#pragma once
#include <fstream>
//
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <fmt/core.h>
#include <glm/glm.hpp>
//
#include "components/vertices.hpp"

struct Grid {
    void init(vma::Allocator vmalloc, uint32_t i_queue) {
		std::ifstream file;
		std::string filepath = "../hashgrid.grid";
		file.open(filepath, std::ifstream::binary);
        if (file.good()) {
            float voxelsize = 0;
            std::size_t n_scan_points = 0;
            std::size_t n_query_points = 0;
            std::size_t n_cells = 0;
			
			file.read(reinterpret_cast<char*>(&voxelsize), sizeof(float));
			file.read(reinterpret_cast<char*>(&n_scan_points), sizeof(std::size_t));
			file.read(reinterpret_cast<char*>(&n_query_points), sizeof(std::size_t));
			file.read(reinterpret_cast<char*>(&n_cells), sizeof(std::size_t));
            
            // read header
            fmt::println("voxelsize of: {} with {} scan points, {} query points and {} cells", voxelsize, n_scan_points, n_query_points, n_cells);
            
            // alloc and read scan points
			std::vector<glm::vec3> scan_points;
            scan_points.reserve(n_scan_points);
            for (std::size_t i = 0; i < scan_points.capacity(); i++) {
                glm::vec3 scan_point;
				file.read(reinterpret_cast<char*>(&scan_point), sizeof(glm::vec3));
                scan_points.push_back(scan_point);
            }
			_scan_points.init(vmalloc, i_queue, scan_points);

			// alloc and read query points
			std::vector<std::pair<glm::vec3, float>> query_points;
            query_points.reserve(n_query_points);
            for (std::size_t i = 0; i < query_points.capacity(); i++) {
                glm::vec3 position;
				float signed_distance;
				file.read(reinterpret_cast<char*>(&position), sizeof(glm::vec3));
				file.read(reinterpret_cast<char*>(&signed_distance), sizeof(float));
                query_points.emplace_back(position, signed_distance * (1.0f / voxelsize));
            }
			_query_points.init(vmalloc, i_queue, query_points);

            file.close();
        }
        else {
            fmt::println("unable to read grid: {}", filepath);
            return;
        }
    }
    void destroy(vma::Allocator vmalloc) {
		_scan_points.destroy(vmalloc);
		_query_points.destroy(vmalloc);
		_cells.destroy(vmalloc);
    }
    
public:
    Vertices<glm::vec3> _scan_points;
    Vertices<std::pair<glm::vec3, float>> _query_points;
    Vertices<std::array<uint32_t, 8>> _cells;
};