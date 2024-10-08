#pragma once
#include <cstdint>
#include <utility>
#include <span>
#include <vulkan/vulkan.hpp>

namespace smaa {
    std::span<const std::byte> get_search_tex();
    std::span<const std::byte> get_area_tex();
    vk::Extent3D get_search_extent();
    vk::Extent3D get_area_extent();
}