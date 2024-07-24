#pragma once
#include <set>
#include <bit>
//
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
//
#include "core/queues.hpp"

struct DeviceSelector {
    auto select_physical_device(vk::Instance instance) -> vk::PhysicalDevice {
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
            // favor gpu if a is discrete and b is not
            if (std::get<1>(a) < std::get<1>(b)) return false;
            // else compare total local memory of all heaps
            else return std::get<2>(a) > std::get<2>(b);
        };
        std::sort(matching_devices.begin(), matching_devices.end(), fnc_sorter);
        vk::PhysicalDevice phys_device = std::get<0>(matching_devices.front());
        fmt::println("Picked device: {}", (const char*)phys_device.getProperties().deviceName);
        return phys_device;
    }
    auto create_logical_device(vk::PhysicalDevice physical_device) -> std::pair<vk::Device, std::vector<uint32_t>> {

        // set up chain of requested device features
        vk::PhysicalDeviceFeatures2 required_features {
            .pNext = &_required_vk11_features,
            .features = _required_features,
        };
        _required_vk11_features.pNext = &_required_vk12_features;
        _required_vk12_features.pNext = &_required_vk13_features;
        
        // create device
        auto [info_queues, queue_mappings] = create_queue_infos(physical_device);
        vk::DeviceCreateInfo info_device {
            .pNext = &required_features,
            .queueCreateInfoCount = (uint32_t)info_queues.size(),
            .pQueueCreateInfos = info_queues.data(),
            .enabledExtensionCount = (uint32_t)_required_extensions.size(),
            .ppEnabledExtensionNames = _required_extensions.data(),
        };

        vk::Device device = physical_device.createDevice(info_device);
        return std::make_pair(device, queue_mappings);
    }

private:
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
        typedef std::array<bool, sizeof(vk::PhysicalDeviceFeatures)> FeatureArray;
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
        typedef std::array<bool, sizeof(vk::PhysicalDeviceVulkan11Features)> FeatureArray;
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
        typedef std::array<bool, sizeof(vk::PhysicalDeviceVulkan12Features)> FeatureArray;
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
        typedef std::array<bool, sizeof(vk::PhysicalDeviceVulkan13Features)> FeatureArray;
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
    auto create_queue_infos(vk::PhysicalDevice physical_device) -> std::pair<std::vector<vk::DeviceQueueCreateInfo>, std::vector<uint32_t>> {
        auto queue_families = physical_device.getQueueFamilyProperties();
        // first: queue family index, second: count of queue capabilities
        typedef std::pair<uint32_t, uint32_t> QueueCount;
        std::vector<std::vector<QueueCount>> queue_family_counters { 
            _required_queues.size()
        };

        // iterate through all queue families to store relevant ones
        for (size_t i = 0; i < queue_families.size(); i++) {
            vk::QueueFlags flags = queue_families[i].queueFlags;
            uint32_t capability_count = std::popcount((uint32_t)flags);
            // check if this queue family works for each requested queue
            for (size_t q = 0; q < _required_queues.size(); q++) {
                vk::QueueFlags masked = flags & _required_queues[q];
                if (masked == _required_queues[q]) {
                    queue_family_counters[q].emplace_back(i, capability_count);
                }
            }
        }

        // favor queues with the least number of capabilities
        auto fnc_sorter = [](QueueCount& a, QueueCount& b) {
            return a.second < b.second;
        };
        // default queue priority
        float priority = 1.0f;

        // contains queue family indices for requested queues
        std::vector<uint32_t> queue_mappings(_required_queues.size());
        // set up queue create infos with unique family indices
        std::vector<vk::DeviceQueueCreateInfo> info_queues;
        for (size_t i = 0; i < _required_queues.size(); i++) {
            // get the queue with the least flags possible
            auto& vec = queue_family_counters[i];
            std::sort(vec.begin(), vec.end(), fnc_sorter);
            auto [queue_family_index, _] = vec.front();

            // map the requested queue to info_queues
            queue_mappings[i] = info_queues.size();
            
            // emplace each queue family only once
            for (size_t q = 0; q < info_queues.size(); q++) {
                uint32_t family = info_queues[q].queueFamilyIndex;
                // when queue family is already present, update mapping
                if (family == queue_family_index) {
                    queue_mappings[i] = q;
                    break;
                }
            }

            // when unique, emplace a new queue family info
            if (queue_mappings[i] == info_queues.size()) {
                info_queues.push_back({
                    .queueFamilyIndex = queue_family_index,
                    .queueCount = 1,
                    .pQueuePriorities = &priority,
                });
            }
        }
        return std::make_pair(info_queues, queue_mappings);
    }

public:
    uint32_t _required_major;
    uint32_t _required_minor;
    std::vector<const char*> _required_extensions;
    vk::PhysicalDeviceFeatures _required_features;
    vk::PhysicalDeviceVulkan11Features _required_vk11_features;
    vk::PhysicalDeviceVulkan12Features _required_vk12_features;
    vk::PhysicalDeviceVulkan13Features _required_vk13_features;
    std::vector<vk::QueueFlags> _required_queues;
};