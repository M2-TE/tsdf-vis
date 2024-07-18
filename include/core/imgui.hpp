#pragma once
#include <vulkan/vulkan.hpp>
#include <SDL3/SDL_events.h>
#include <imgui.h>
//
#include "core/queues.hpp"

namespace ImGui 
{
    namespace utils {
        static void display_fps() {
            ImGui::SetNextWindowBgAlpha(0.35f);
            ImGui::Begin("FPS_Overlay", nullptr, ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_NoDocking
                | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoFocusOnAppearing
                | ImGuiWindowFlags_AlwaysAutoResize
                | ImGuiWindowFlags_NoNav
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoMouseInputs);
            ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
            ImGui::Text("%.1f ms", ImGui::GetIO().DeltaTime * 1000.0f);
            ImGui::End();
        }
	}
    namespace impl
    {
        void init_sdl(SDL_Window* p_window);
        void init_vulkan(vk::Instance instance, vk::Device device, vk::PhysicalDevice phys_device, Queues& queues, vk::Format color_format);
        bool process_event(const SDL_Event* p_event);
        void new_frame();
        void draw(vk::CommandBuffer cmd, vk::ImageView& image_view, vk::ImageLayout layout, vk::Extent2D extent);
        void shutdown(vk::Device device);
    }
}