#pragma once

struct Plymesh {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues, std::string_view path_rel, std::optional<glm::vec3> color = std::nullopt) {
        std::ifstream file;
        std::string path_full = SDL_GetBasePath();
        path_full.append(path_rel.data());
		file.open(path_full, std::ifstream::binary);
        if (file.good()) {
            std::string line;

            // validate header start
            std::getline(file, line);
            if (line != "ply") {
                fmt::println("corrupted header for {}", path_full);
            }
            
            // ignoring format for now
            std::getline(file, line);

            // element vertex N
            std::getline(file, line);
            size_t str_pos = line.find_last_of(' ');
            std::string vert_n_str = line.substr(str_pos + 1, line.size() - str_pos);
            size_t vert_n = std::stoi(vert_n_str);
            
            // skip parsing vertex contents for now
            for (size_t i = 0; i < 6; i++) {
                std::getline(file, line);
            }

            // element face N
            std::getline(file, line);
            str_pos = line.find_last_of(' ');
            std::string face_n_str = line.substr(str_pos + 1, line.size() - str_pos);
            size_t face_n = std::stoi(face_n_str);

            // skip face properties
            std::getline(file, line);

            // validate header end
            std::getline(file, line);
            if (line != "end_header") {
                fmt::println("missing/unexpected header end for {}", path_full);
            }

            // read little endian vertices
            typedef std::pair<glm::vec3, glm::vec3> RawVertex;
            std::vector<RawVertex> raw_vertices(vert_n);
            file.read(reinterpret_cast<char*>(raw_vertices.data()), sizeof(RawVertex) * raw_vertices.size());
            // flip y and swap y with z and insert into full vertex vector
            std::vector<Vertex> vertices;
            vertices.reserve(vert_n);
            for (auto& vertex: raw_vertices) {
                glm::vec4 pos { vertex.first.x, -vertex.first.z, vertex.first.y, 1 };
                glm::vec4 norm { vertex.second.x, vertex.second.y, vertex.second.z, 0 };
                glm::vec4 col = color.has_value() ? glm::vec4(color.value(), 1) : norm;
                vertices.emplace_back(pos, norm, col);
            }

            // read little endian faces into indices
            std::vector<uint32_t> raw_indices;
            raw_indices.reserve(face_n * 3);
            for (size_t i = 0; i < face_n; i++) {
                uint8_t indices_n; // number of indices per face
                file.read(reinterpret_cast<char*>(&indices_n), sizeof(uint8_t));
                std::array<int, 3> indices_face;
                file.read(reinterpret_cast<char*>(&indices_face), sizeof(int) * 3);
                raw_indices.insert(raw_indices.end(), indices_face.cbegin(), indices_face.cend());
            }
            file.close();
            
            // create actual mesh from raw data
            _mesh.init(vmalloc, queues, vertices, raw_indices);
        }
        else {
            fmt::println("failed to load ply file: {}", path_full);
        }
    }
    void destroy(vma::Allocator vmalloc) {
        _mesh.destroy(vmalloc);
    }

    struct Vertex {
        glm::vec4 pos;
        glm::vec4 norm;
        glm::vec4 color;
    };
    typedef uint32_t Index;
    Mesh<Vertex, Index> _mesh;
};