#pragma once

struct Plymesh {
    void init(vma::Allocator vmalloc, const vk::ArrayProxy<uint32_t>& queues, std::string_view path) {
        std::ifstream file;
		file.open(path.data(), std::ifstream::binary);
        if (file.good()) {
            std::string line;

            // validate header start
            std::getline(file, line);
            if (line != "ply") {
                fmt::println("corrupted header for {}", path.data());
            }
            
            // ignoring format for now
            std::getline(file, line);

            // element vertex N
            std::getline(file, line);
            size_t pos = line.find_last_of(' ');
            std::string vert_n_str = line.substr(pos + 1, line.size() - pos);
            size_t vert_n = std::stoi(vert_n_str);
            
            // skip parsing vertex contents for now
            for (size_t i = 0; i < 6; i++) {
                std::getline(file, line);
            }

            // element face N
            std::getline(file, line);
            pos = line.find_last_of(' ');
            std::string face_n_str = line.substr(pos + 1, line.size() - pos);
            size_t face_n = std::stoi(face_n_str);

            // skip face properties
            std::getline(file, line);

            // validate header end
            std::getline(file, line);
            if (line != "end_header") {
                fmt::println("missing/unexpected header end for {}", path.data());
            }

            // read little endian vertices
            std::vector<Vertex> raw_vertices(vert_n);
            file.read(reinterpret_cast<char*>(raw_vertices.data()), sizeof(Vertex) * raw_vertices.size());
            // flip y and swap y with z
            for (auto& vertex: raw_vertices) {
                std::swap(vertex.first.y, vertex.first.z);
                vertex.first.y *= -1.0;
            }

            // read little endian faces into indices
            std::vector<uint32_t> raw_indices;
            raw_indices.reserve(face_n * 3);
            for (size_t i = 0; i < face_n; i++) {
                u_char indices_n; // number of indices per face
                file.read(reinterpret_cast<char*>(&indices_n), sizeof(u_char));
                std::array<int, 3> indices_face;
                file.read(reinterpret_cast<char*>(&indices_face), sizeof(int) * 3);
                raw_indices.insert(raw_indices.end(), indices_face.cbegin(), indices_face.cend());
            }
            file.close();
            
            // create actual mesh from raw data
            _mesh.init(vmalloc, queues, raw_vertices, raw_indices);
        }
        else {
            fmt::println("failed to load ply file: {}", path.data());
        }
    }
    void destroy(vma::Allocator vmalloc) {
        _mesh.destroy(vmalloc);
    }

    typedef std::pair<glm::vec3, glm::vec3> Vertex;
    typedef uint32_t Index;
    Mesh<Vertex, Index> _mesh;
};