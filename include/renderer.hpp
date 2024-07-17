#pragma once
#include <cstdint>
//
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
//
#include "image.hpp"

class Renderer {
    struct FrameData {
        vk::CommandPool command_pool;
        vk::CommandBuffer command_buffer;
        vk::Semaphore timeline;
        uint64_t timeline_val;
    };
public:
    void init() {
        
    }
    void destroy() {}
    
private:
    void draw() {}
    
private:
    Image dst_image;
    // Pipeline::Graphics pipe_graphics;
    // Pipeline::Compute pipe_compute;
};