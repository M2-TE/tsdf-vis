#define VULKAN_HPP_USE_REFLECT
#include <vulkan/vulkan.hpp>
#include "core/device_selector.hpp"

bool DeviceSelector::check_api_ver(vk::PhysicalDeviceProperties& props) {
    bool passed = true;
    if (vk::apiVersionMajor(props.apiVersion) > _required_major) passed = false;
    if (vk::apiVersionMinor(props.apiVersion) > _required_minor) passed = false;
    if (!passed) fmt::println("\tMissing vulkan {}.{} support", _required_major, _required_minor);
    return passed;
}
bool DeviceSelector::check_extensions(std::set<std::string> required_extensions, vk::PhysicalDevice physical_device) {
    auto ext_props = physical_device.enumerateDeviceExtensionProperties();
    for (auto& extension: ext_props) {
        std::string ext_name = extension.extensionName;
        auto ext_it = required_extensions.find(ext_name);
        // erase from set if found
        if (ext_it != required_extensions.end()) {
            required_extensions.erase(ext_it);
        }
    }
    // print missing extensions, if any
    for (auto& extension: required_extensions) {
        fmt::println("\tMissing device extension: {}", extension);
    }
    // pass if all required extensions were erased
    bool passed = required_extensions.size() == 0;
    return passed;

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
    if (!passed) fmt::println("\tMissing vulkan 1.0 device features");
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

    if (!passed) fmt::println("\tMissing vulkan 1.1 device features");
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
    if (!passed) fmt::println("\tMissing vulkan 1.2 device features");
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
    if (!passed) fmt::println("\tMissing vulkan 1.3 device features");
    return passed;
}
