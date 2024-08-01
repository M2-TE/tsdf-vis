#define VULKAN_HPP_USE_REFLECT
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>
#include "util/device_selector.hpp"

bool DeviceSelector::check_api_ver(vk::PhysicalDeviceProperties& props) {
    bool passed_version = true;
    if (vk::apiVersionMajor(props.apiVersion) > _required_major) passed_version = false;
    if (vk::apiVersionMinor(props.apiVersion) > _required_minor) passed_version = false;
    if (passed_version) fmt::print("[api] ");
    else                fmt::print("[!api] ");
    return passed_version;
}
bool DeviceSelector::check_extensions(std::set<std::string>& required_extensions, vk::PhysicalDevice physical_device) {
    size_t matches_n = 0;
    auto ext_props = physical_device.enumerateDeviceExtensionProperties();
    for (auto& extension: ext_props) {
        std::string ext_name = extension.extensionName;
        if (required_extensions.contains(ext_name)) matches_n++;
    }
    bool passed_extensions = false;
    if (matches_n == _required_extensions.size()) passed_extensions = true;
    if (passed_extensions) fmt::print("[ext] ");
    else                   fmt::print("[!ext] ");
    return passed_extensions;

}
bool DeviceSelector::check_features(vk::PhysicalDeviceFeatures& features) {
    // track available features via simple vector
    std::vector<vk::Bool32> available_features;
    auto add_available = [&](auto arg) {
        if constexpr (std::is_same_v<decltype(arg), vk::Bool32>) {
            available_features.push_back(arg);
        }
    };
    // iterate over available features
    std::apply([&](auto&... args) {
        ((add_available(args)), ...);
    }, features.reflect());

    // track required features via simple vector
    std::vector<vk::Bool32> required_features;
    auto add_required = [&](auto arg) {
        if constexpr (std::is_same_v<decltype(arg), vk::Bool32>) {
            required_features.push_back(arg);
        }
    };
    // iterate over required features
    std::apply([&](auto&... args) {
        ((add_required(args)), ...);
    }, _required_features.reflect());

    // check if all required features are available
    bool passed = true;
    for (std::size_t i = 0; i < available_features.size(); i++) {
        if (required_features[i] && !available_features[i]) passed = false;
    }

    if (passed) fmt::print("[feat] ");
    else        fmt::print("[!feat] ");
    return passed;
}
bool DeviceSelector::check_features(vk::PhysicalDeviceVulkan11Features& features) {
    // track available features via simple vector
    std::vector<vk::Bool32> available_features;
    auto add_available = [&](auto arg) {
        if constexpr (std::is_same_v<decltype(arg), vk::Bool32>) {
            available_features.push_back(arg);
        }
    };
    // iterate over available features
    std::apply([&](auto&... args) {
        ((add_available(args)), ...);
    }, features.reflect());

    // track required features via simple vector
    std::vector<vk::Bool32> required_features;
    auto add_required = [&](auto arg) {
        if constexpr (std::is_same_v<decltype(arg), vk::Bool32>) {
            required_features.push_back(arg);
        }
    };
    // iterate over required features
    std::apply([&](auto&... args) {
        ((add_required(args)), ...);
    }, _required_vk11_features.reflect());

    // check if all required features are available
    bool passed = true;
    for (std::size_t i = 0; i < available_features.size(); i++) {
        if (required_features[i] && !available_features[i]) passed = false;
    }

    if (passed) fmt::print("[feat11] ");
    else        fmt::print("[!feat11] ");
    return passed;
}
bool DeviceSelector::check_features(vk::PhysicalDeviceVulkan12Features& features) {
    // track available features via simple vector
    std::vector<vk::Bool32> available_features;
    auto add_available = [&](auto arg) {
        if constexpr (std::is_same_v<decltype(arg), vk::Bool32>) {
            available_features.push_back(arg);
        }
    };
    // iterate over available features
    std::apply([&](auto&... args) {
        ((add_available(args)), ...);
    }, features.reflect());

    // track required features via simple vector
    std::vector<vk::Bool32> required_features;
    auto add_required = [&](auto arg) {
        if constexpr (std::is_same_v<decltype(arg), vk::Bool32>) {
            required_features.push_back(arg);
        }
    };
    // iterate over required features
    std::apply([&](auto&... args) {
        ((add_required(args)), ...);
    }, _required_vk12_features.reflect());

    // check if all required features are available
    bool passed = true;
    for (std::size_t i = 0; i < available_features.size(); i++) {
        if (required_features[i] && !available_features[i]) passed = false;
    }
    if (passed) fmt::print("[feat12] ");
    else        fmt::print("[!feat12] ");
    return passed;
}
bool DeviceSelector::check_features(vk::PhysicalDeviceVulkan13Features& features) {
    // track available features via simple vector
    std::vector<vk::Bool32> available_features;
    auto add_available = [&](auto arg) {
        if constexpr (std::is_same_v<decltype(arg), vk::Bool32>) {
            available_features.push_back(arg);
        }
    };
    // iterate over available features
    std::apply([&](auto&... args) {
        ((add_available(args)), ...);
    }, features.reflect());

    // track required features via simple vector
    std::vector<vk::Bool32> required_features;
    auto add_required = [&](auto arg) {
        if constexpr (std::is_same_v<decltype(arg), vk::Bool32>) {
            required_features.push_back(arg);
        }
    };
    // iterate over required features
    std::apply([&](auto&... args) {
        ((add_required(args)), ...);
    }, _required_vk13_features.reflect());

    // check if all required features are available
    bool passed = true;
    for (std::size_t i = 0; i < available_features.size(); i++) {
        if (required_features[i] && !available_features[i]) passed = false;
    }
    if (passed) fmt::print("[feat13] ");
    else        fmt::print("[!feat13] ");
    return passed;
}
