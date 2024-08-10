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
}