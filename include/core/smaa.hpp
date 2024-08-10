#pragma once
#include <cstdint>
#include <utility>
#include <span>

namespace smaa {
    std::span<const std::byte> get_search_tex();
    std::span<const std::byte> get_area_tex();
}