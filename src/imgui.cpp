#include <vulkan/vulkan.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_events.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <fmt/core.h>
//
#include "queues.hpp"

namespace ImGui 
{
    namespace impl
    {
        static vk::DescriptorPool imgui_desc_pool;
        static auto load_fnc(const char* p_function_name, void* p_user_data) -> PFN_vkVoidFunction {
            const vk::Instance instance = *reinterpret_cast<vk::Instance*>(p_user_data);
            VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr(nullptr, p_function_name);
            return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(instance, p_function_name);
        }
        void init_sdl(SDL_Window* p_window) {
            ImGui::CreateContext();
            ImGui_ImplSDL3_InitForVulkan(p_window);
        }
        void init_vulkan(vk::Instance instance, vk::Device device, vk::PhysicalDevice phys_device, Queues& queues, vk::Format color_format) {
            bool success = ImGui_ImplVulkan_LoadFunctions(&load_fnc, &instance);
            if (!success) fmt::println("imgui failed to load vulkan functions");

            // create temporary descriptor pool for sdl
            std::vector<vk::DescriptorPoolSize> pool_sizes = {
                vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eUniformTexelBuffer, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eStorageTexelBuffer, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, 1000),
                vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 1000)
            };
            vk::DescriptorPoolCreateInfo info_desc_pool {
                .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = 1000,
                .poolSizeCount = (uint32_t)pool_sizes.size(), 
                .pPoolSizes = pool_sizes.data(),
            };
            imgui_desc_pool = device.createDescriptorPool(info_desc_pool);

            // initialize vulkan backend
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
            ImGui_ImplVulkan_InitInfo info_imgui_vk {
                .Instance = instance,
                .PhysicalDevice = phys_device,
                .Device = device,
                .Queue = queues.graphics,
                .DescriptorPool = imgui_desc_pool,
                .RenderPass = nullptr,
                .MinImageCount = 3,
                .ImageCount = 3,
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .UseDynamicRendering = true,
                .PipelineRenderingCreateInfo { 
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                    .colorAttachmentCount = 1, 
                    .pColorAttachmentFormats = (VkFormat*)&color_format 
                }
            };
            ImGui_ImplVulkan_Init(&info_imgui_vk);
            ImGui_ImplVulkan_CreateFontsTexture();
        }
        bool process_event(const SDL_Event* p_event) {
            return ImGui_ImplSDL3_ProcessEvent(p_event);
        }
        void new_frame() {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
        }
        void draw(vk::CommandBuffer cmd, vk::ImageView& image_view, vk::ImageLayout layout, vk::Extent2D extent) {
            vk::RenderingAttachmentInfo info_render_attach = {
                .imageView = image_view,
                .imageLayout = layout,
                .loadOp = vk::AttachmentLoadOp::eLoad,
                .storeOp = vk::AttachmentStoreOp::eStore,
            };
            vk::RenderingInfo info_render {
                .renderArea { .offset { .x = 0, .y = 0 }, .extent = extent },
                .layerCount = 1,
                .colorAttachmentCount = 1, .pColorAttachments = &info_render_attach
            };
            cmd.beginRendering(info_render);
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
            cmd.endRendering();
        }
        void shutdown(vk::Device device) {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            device.destroyDescriptorPool(imgui_desc_pool);
        }
    }
}