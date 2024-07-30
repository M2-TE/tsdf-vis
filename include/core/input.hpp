#pragma once
#include <string_view>
#include <cstdint>
#include <set>
#include <SDL3/SDL_events.h>
#if __has_include(<imgui.h>)
#   include <imgui.h>
#   define IMGUI_CAPTURE_EVAL ImGui::GetIO().WantCaptureKeyboard
#else
#   define IMGUI_CAPTURE_EVAL 0
#endif

namespace Input {
	struct Data {
		static auto get() noexcept -> Data& { 
            static Data instance;
            return instance;
        }
		std::set<SDL_Keycode> keys_pressed;
		std::set<SDL_Keycode> keys_down;
		std::set<SDL_Keycode> keys_released;
		std::set<uint8_t> buttons_pressed;
		std::set<uint8_t> buttons_down;
		std::set<uint8_t> buttons_released;
		float x, y;
		float dx, dy;
	};

	struct Keys {
        static inline bool pressed (std::string_view characters) noexcept {
            for (auto cur = characters.cbegin(); cur < characters.cend(); cur++) {
                if (pressed(*cur)) return true;
            }
            return false;
        }
		static inline bool pressed(char character) noexcept { return Data::get().keys_pressed.contains(character) || Data::get().keys_pressed.contains(character + 32); }
		static inline bool pressed(SDL_Keycode code) noexcept { return Data::get().keys_pressed.contains(code); }
        static inline bool down(std::string_view characters) noexcept {
            for (auto cur = characters.cbegin(); cur < characters.cend(); cur++) {
                if (down(*cur)) return true;
            }
            return false;
        }
		static inline bool down(char character) noexcept { return Data::get().keys_down.contains(character) || Data::get().keys_pressed.contains(character + 32); }
		static inline bool down(SDL_Keycode code) noexcept { return Data::get().keys_down.contains(code); }
        static inline bool released(std::string_view characters) noexcept {
            for (auto cur = characters.cbegin(); cur < characters.cend(); cur++) {
                if (released(*cur)) return true;
            }
            return false;
        }
		static inline bool released(char character) noexcept { return Data::get().keys_released.contains(character) || Data::get().keys_pressed.contains(character + 32); }
		static inline bool released(SDL_Keycode code) noexcept { return Data::get().keys_released.contains(code); }
    };
	struct Mouse {
		struct ids { static constexpr uint8_t left = SDL_BUTTON_LEFT, right = SDL_BUTTON_RIGHT, middle = SDL_BUTTON_MIDDLE; };
		static inline bool pressed(uint8_t button_id) noexcept { return Data::get().buttons_pressed.contains(button_id); }
		static inline bool down(uint8_t button_id) noexcept { return Data::get().buttons_down.contains(button_id); }
		static inline bool released(uint8_t button_id) noexcept { return Data::get().buttons_released.contains(button_id); }
		static inline auto position() noexcept -> std::pair<decltype(Data::x), decltype(Data::y)> { return std::pair(Data::get().x, Data::get().y); };
		static inline auto delta() noexcept -> std::pair<decltype(Data::dx), decltype(Data::dy)> { return std::pair(Data::get().dx, Data::get().dy); };
	};
	
    static void flush() noexcept {
		Data::get().keys_pressed.clear();
		Data::get().keys_released.clear();
		Data::get().buttons_pressed.clear();
		Data::get().buttons_released.clear();
		Data::get().dx = 0;
		Data::get().dy = 0;
	}
	static void flush_all() noexcept {
		flush();
		Data::get().keys_down.clear();
		Data::get().buttons_down.clear();
	}
	static void register_key_up(const SDL_KeyboardEvent& key_event) noexcept {
		if (key_event.repeat || IMGUI_CAPTURE_EVAL) return;
		Data::get().keys_released.insert(key_event.key);
		Data::get().keys_down.erase(key_event.key);
	}
	static void register_key_down(const SDL_KeyboardEvent& key_event) noexcept {
		if (key_event.repeat || IMGUI_CAPTURE_EVAL) return;
		Data::get().keys_pressed.insert(key_event.key);
		Data::get().keys_down.insert(key_event.key);
	}
	static void register_button_up(const SDL_MouseButtonEvent& button_event) noexcept {
		// if (IMGUI_CAPTURE_EVAL) return;
		Data::get().buttons_released.insert(button_event.button);
		Data::get().buttons_down.erase(button_event.button);
	}
	static void register_button_down(const SDL_MouseButtonEvent& button_event) noexcept {
		// if (IMGUI_CAPTURE_EVAL) return;
		Data::get().buttons_pressed.insert(button_event.button);
		Data::get().buttons_down.insert(button_event.button);
	}
	static void register_motion(const SDL_MouseMotionEvent& motion_event) noexcept {
		Data::get().dx += motion_event.xrel;
		Data::get().dy += motion_event.yrel;
		Data::get().x += motion_event.xrel;
		Data::get().y += motion_event.yrel;
	}
}
#undef IMGUI_CAPTURE_EVAL
typedef Input::Keys Keys;
typedef Input::Mouse Mouse;