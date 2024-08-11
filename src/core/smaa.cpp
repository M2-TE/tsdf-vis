#include "core/smaa.hpp"
// embedded SMAA lookup textures
#include "AreaTex.h"
#include "SearchTex.h"

namespace smaa {
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