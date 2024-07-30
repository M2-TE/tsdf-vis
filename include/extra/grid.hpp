#pragma once
#include <fstream>
#include <vector>
#include <array>
//
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <fmt/core.h>
#include <glm/glm.hpp>
//
#include "components/mesh.hpp"
#include "components/vertices.hpp"
#include "components/indices.hpp"

struct Grid {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues, std::string_view path) {
		std::ifstream file;
		file.open(path.data(), std::ifstream::binary);
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
            for (std::size_t i = 0; i < n_scan_points; i++) {
                glm::vec3 scan_point;
				file.read(reinterpret_cast<char*>(&scan_point), sizeof(glm::vec3));
                std::swap(scan_point.y, scan_point.z);
                scan_point.y *= -1.0;
                scan_points.push_back(scan_point);
            }
			_scan_points.init(vmalloc, queues, scan_points);

			// alloc and read query points
			std::vector<QueryPoint> query_points;
            query_points.reserve(n_query_points);
            for (std::size_t i = 0; i < n_query_points; i++) {
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
            cell_indices.reserve(n_cells * 8); // TODO: multiply with indices per cell
            for (std::size_t i = 0; i < n_cells; i++) {
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
            fmt::println("unable to read grid: {}", path.data());
        }
    }
    void destroy(vma::Allocator vmalloc) {
		_scan_points.destroy(vmalloc);
		_query_points.destroy(vmalloc);
    }
    
public:
    typedef uint32_t Index;
    typedef glm::vec3 ScanPoint;
    typedef std::pair<glm::vec3, float> QueryPoint;
    Mesh<ScanPoint> _scan_points; // loose points
    Mesh<QueryPoint, Index> _query_points; // indexed line list
};