#pragma once
#include <utility>
#include <limits>
#include <set>
//
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>

struct DeviceSelector {
    void select(vk::Instance instance) {
        auto matching_devices = check_physical_devices(instance);

        // create logical device and check for feature support
        // for (auto& dev_entry: matching_devices) {
        //     vk::PhysicalDevice phys_device = std::get<0>(dev_entry);


            
        //     // enumerate queue families (todo: move into Queues::init())
        //     uint32_t i_invalid = std::numeric_limits<uint32_t>().max();
        //     uint32_t i_universal = i_invalid;
        //     uint32_t i_graphics = i_invalid;
        //     uint32_t i_compute = i_invalid;
        //     uint32_t i_transfer = i_invalid;
        //     auto queue_props = phys_device.getQueueFamilyProperties();
        //     for (size_t i = 0; i < queue_props.size(); i++) {
                
        //     }
        // }
    }

private:
    auto check_physical_devices(vk::Instance instance) -> std::vector<std::tuple<vk::PhysicalDevice, bool, vk::DeviceSize>> {
        // bool: is_discrete, vk::DeviceSize: memory_size
        std::vector<std::tuple<vk::PhysicalDevice, bool, vk::DeviceSize>> matching_devices;

        // enumerate devices
        auto phys_devices = instance.enumeratePhysicalDevices();
        if (phys_devices.size() == 0) fmt::println("No device with vulkan support found");

        // create a std::set from the required extensions
        std::set<std::string> required_extensions {
            _required_extensions.cbegin(),
            _required_extensions.cend()
        };
        
        // check for matching devices
        fmt::println("Available devices:");
        for (vk::PhysicalDevice device: phys_devices) {
            auto props = device.getProperties();
            fmt::print("\t");

            // check vulkan api version
            bool passed_version = check_api_ver(props);
            // check requested extensions
            bool passed_extensions = check_extensions(required_extensions, device);
            // retrieve available core features
            auto feature_chain = device.getFeatures2<
                vk::PhysicalDeviceFeatures2,
                vk::PhysicalDeviceVulkan11Features,
                vk::PhysicalDeviceVulkan12Features,
                vk::PhysicalDeviceVulkan13Features>();
            bool passed_features = true;
            passed_features &= check_features(feature_chain.get<vk::PhysicalDeviceFeatures2>().features);
            passed_features &= check_features(feature_chain.get<vk::PhysicalDeviceVulkan11Features>());
            passed_features &= check_features(feature_chain.get<vk::PhysicalDeviceVulkan12Features>());
            passed_features &= check_features(feature_chain.get<vk::PhysicalDeviceVulkan13Features>());
            // check if all tests were passed
            if (passed_version && passed_extensions && passed_features) {
                bool is_discrete = props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
                vk::DeviceSize memory_size = 0;
                auto memory_props = device.getMemoryProperties();
                for (auto& heap: memory_props.memoryHeaps) {
                    if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal) {
                        memory_size += heap.size;
                    }
                }
                matching_devices.emplace_back(device, is_discrete, memory_size);
            }
            const char* dev_name = props.deviceName;
            fmt::println("-> {}", dev_name);
        }
        // sort devices by favouring discrete gpus and large local memory heaps
        typedef std::tuple<vk::PhysicalDevice, bool, vk::DeviceSize> DeviceEntry;
        auto fnc_sorter = [](DeviceEntry& a, DeviceEntry& b) {
            // favor a if it is a discrete GPU and b is not
            if (std::get<1>(a) && !std::get<1>(b)) return true;
            // compare total local memory heap size
            if (std::get<2>(a) > std::get<2>(b)) return true;
            return false;
        };
        std::sort(matching_devices.begin(), matching_devices.end(), fnc_sorter);
        return matching_devices;
    }
    bool check_api_ver(vk::PhysicalDeviceProperties& props) {
        bool passed_version = true;
        if (vk::apiVersionMajor(props.apiVersion) > _required_major) passed_version = false;
        if (vk::apiVersionMinor(props.apiVersion) > _required_minor) passed_version = false;
        if (passed_version) fmt::print("[api] ");
        else                fmt::print("[!api] ");
        return passed_version;
    }
    bool check_extensions(std::set<std::string>& required_extensions, vk::PhysicalDevice device) {
        size_t n_matches = 0;
        auto ext_props = device.enumerateDeviceExtensionProperties();
        for (auto& extension: ext_props) {
            std::string ext_name = extension.extensionName;
            if (required_extensions.contains(ext_name)) n_matches++;
        }
        bool passed_extensions = false;
        if (n_matches == _required_extensions.size()) passed_extensions = true;
        if (passed_extensions) fmt::print("[ext] ");
        else                   fmt::print("[!ext] ");
        return passed_extensions;

    }
    bool check_features(vk::PhysicalDeviceFeatures& features) {
        typedef std::array<bool, sizeof(vk::PhysicalDeviceFeatures) * sizeof(bool)> FeatureArray;
        FeatureArray* available_features = reinterpret_cast<FeatureArray*>(&features);
        FeatureArray* required_features = reinterpret_cast<FeatureArray*>(&_required_features);
        bool passed_features = true;
        for (size_t i = 0; i < available_features->size(); i++) {
            bool required = (*required_features)[i];
            bool available = (*available_features)[i];
            if (required && !available) passed_features = false;
        }
        if (passed_features) fmt::print("[feat] ");
        else                 fmt::print("[!feat] ");
        return passed_features;
    }
    bool check_features(vk::PhysicalDeviceVulkan11Features& features) {
        typedef std::array<bool, sizeof(vk::PhysicalDeviceVulkan11Features) * sizeof(bool)> FeatureArray;
        FeatureArray* available_features = reinterpret_cast<FeatureArray*>(&features);
        FeatureArray* required_features = reinterpret_cast<FeatureArray*>(&_required_vk11_features);
        bool passed_features = true;
        for (size_t i = 0; i < available_features->size(); i++) {
            bool required = (*required_features)[i];
            bool available = (*available_features)[i];
            if (required && !available) passed_features = false;
        }
        if (passed_features) fmt::print("[feat11] ");
        else                 fmt::print("[!feat11] ");
        return passed_features;
    }
    bool check_features(vk::PhysicalDeviceVulkan12Features& features) {
        typedef std::array<bool, sizeof(vk::PhysicalDeviceVulkan12Features) * sizeof(bool)> FeatureArray;
        FeatureArray* available_features = reinterpret_cast<FeatureArray*>(&features);
        FeatureArray* required_features = reinterpret_cast<FeatureArray*>(&_required_vk12_features);
        bool passed_features = true;
        for (size_t i = 0; i < available_features->size(); i++) {
            bool required = (*required_features)[i];
            bool available = (*available_features)[i];
            if (required && !available) passed_features = false;
        }
        if (passed_features) fmt::print("[feat12] ");
        else                 fmt::print("[!feat12] ");
        return passed_features;
    }
    bool check_features(vk::PhysicalDeviceVulkan13Features& features) {
        typedef std::array<bool, sizeof(vk::PhysicalDeviceVulkan13Features) * sizeof(bool)> FeatureArray;
        FeatureArray* available_features = reinterpret_cast<FeatureArray*>(&features);
        FeatureArray* required_features = reinterpret_cast<FeatureArray*>(&_required_vk13_features);
        bool passed_features = true;
        for (size_t i = 0; i < available_features->size(); i++) {
            bool required = (*required_features)[i];
            bool available = (*available_features)[i];
            if (required && !available) passed_features = false;
        }
        if (passed_features) fmt::print("[feat13] ");
        else                 fmt::print("[!feat13] ");
        return passed_features;
    }

public:
    uint32_t _required_major;
    uint32_t _required_minor;
    std::vector<std::string> _required_extensions;
    vk::PhysicalDeviceFeatures _required_features;
    vk::PhysicalDeviceVulkan11Features _required_vk11_features;
    vk::PhysicalDeviceVulkan12Features _required_vk12_features;
    vk::PhysicalDeviceVulkan13Features _required_vk13_features;
};