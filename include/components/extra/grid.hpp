#pragma once
#include <fstream>
#include <vector>
#include <array>
//
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <fmt/base.h>
#include <glm/glm.hpp>
#include "components/mesh/mesh.hpp"
#include "components/mesh/vertices.hpp"
#include "components/mesh/indices.hpp"

struct Grid {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues, std::string_view path_rel) {
		std::ifstream file;
        std::string path_full = SDL_GetBasePath();
        path_full.append(path_rel.data());
		file.open(path_full, std::ifstream::binary);
        if (file.good()) {
            // read header
            float voxelsize = 0;
            std::size_t query_points_n = 0;
            std::size_t cells_n = 0;
			file.read(reinterpret_cast<char*>(&voxelsize), sizeof(float));
			file.read(reinterpret_cast<char*>(&query_points_n), sizeof(std::size_t));
			file.read(reinterpret_cast<char*>(&cells_n), sizeof(std::size_t));
            fmt::println("voxelsize of: {} with {} query points and {} cells", voxelsize, query_points_n, cells_n);
            
			// alloc and read query points
			std::vector<QueryPoint> query_points;
            query_points.reserve(query_points_n);
            for (std::size_t i = 0; i < query_points_n; i++) {
                glm::vec3 position;
				float signed_distance;
				file.read(reinterpret_cast<char*>(&position), sizeof(glm::vec3));
				file.read(reinterpret_cast<char*>(&signed_distance), sizeof(float));
                std::swap(position.y, position.z);
                position.y *= -1.0;
                query_points.emplace_back(position, signed_distance * (1.0f / voxelsize));
            }
            // alloc and read cells (indexing into query points)
            std::vector<Index> cell_indices;
            cell_indices.reserve(cells_n * 8); // TODO: multiply with indices per cell
            for (std::size_t i = 0; i < cells_n; i++) {
                std::array<Index, 8> cell;
				file.read(reinterpret_cast<char*>(cell.data()), sizeof(cell));
                // build cell edge via line strip indices
                // front side
                cell_indices.insert(cell_indices.end(), cell.cbegin() + 0, cell.cbegin() + 4);
                cell_indices.push_back(cell[0]);
                // back side
                cell_indices.insert(cell_indices.end(), cell.cbegin() + 4, cell.cbegin() + 8);
                cell_indices.push_back(cell[4]);
                cell_indices.push_back(std::numeric_limits<Index>().max()); // restart strip
                // missing edges
                cell_indices.push_back(cell[3]);
                cell_indices.push_back(cell[7]);
                cell_indices.push_back(cell[6]);
                cell_indices.push_back(cell[2]);
                cell_indices.push_back(cell[1]);
                cell_indices.push_back(cell[5]);
                cell_indices.push_back(std::numeric_limits<Index>().max()); // restart strip
            }
			_query_points.init(vmalloc, queues, query_points, cell_indices);

            file.close();
        }
        else {
            fmt::println("unable to read grid: {}", path_full);
        }
    }
    void destroy(vma::Allocator vmalloc) {
		_query_points.destroy(vmalloc);
    }
    
public:
    typedef uint32_t Index;
    typedef std::pair<glm::vec3, float> QueryPoint;
    Mesh<QueryPoint, Index> _query_points; // indexed line list
};