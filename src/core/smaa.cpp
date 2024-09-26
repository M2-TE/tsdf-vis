#include "core/smaa.hpp"
// embedded SMAA lookup textures
#include "AreaTex.h"
#include "SearchTex.h"

namespace smaa {
    void flip_texture_vertically(std::vector<std::byte>& texture, uint32_t width, uint32_t height, uint32_t bytes_per_pixel) {
        uint32_t row_size = width * bytes_per_pixel;
        std::vector<std::byte> temp_row(row_size);

        for (uint32_t y = 0; y < height / 2; ++y) {
            uint32_t opposite_y = height - y - 1;
            std::memcpy(temp_row.data(), &texture[y * row_size], row_size);
            std::memcpy(&texture[y * row_size], &texture[opposite_y * row_size], row_size);
            std::memcpy(&texture[opposite_y * row_size], temp_row.data(), row_size);
        }
    }

    std::vector<std::byte> get_flipped_search_tex() {
        std::vector<std::byte> texture(reinterpret_cast<const std::byte*>(searchTexBytes), reinterpret_cast<const std::byte*>(searchTexBytes) + sizeof(searchTexBytes));
        flip_texture_vertically(texture, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1); // Assuming 1 byte per pixel
        return texture;
    }

    std::vector<std::byte> get_flipped_area_tex() {
        std::vector<std::byte> texture(reinterpret_cast<const std::byte*>(areaTexBytes), reinterpret_cast<const std::byte*>(areaTexBytes) + sizeof(areaTexBytes));
        flip_texture_vertically(texture, AREATEX_WIDTH, AREATEX_HEIGHT, 1); // Assuming 1 byte per pixel
        return texture;
    }
    std::span<const std::byte> get_search_tex() {
        return std::span(reinterpret_cast<const std::byte*>(searchTexBytes), sizeof(searchTexBytes));
    }
    std::span<const std::byte> get_area_tex() {
        return std::span(reinterpret_cast<const std::byte*>(areaTexBytes), sizeof(areaTexBytes));
    }
    vk::Extent3D get_search_extent() {
        return { SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1 };
    }
    vk::Extent3D get_area_extent() {
        return { AREATEX_WIDTH, AREATEX_HEIGHT, 1 };
    }
}