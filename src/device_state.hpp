#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

struct DeviceState {
    struct ControllerState {
        int device_id{-1};
        std::array<float, SDL_GAMEPAD_AXIS_COUNT> axes{};
        std::array<Uint8, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
    };

    std::array<Uint8, SDL_SCANCODE_COUNT> keyboard{};
    int mouse_x{0};
    int mouse_y{0};
    int mouse_dx{0};
    int mouse_dy{0};
    uint32_t mouse_buttons{0};
    int mouse_wheel{0};
    glm::vec2 mouse_norm{0.0f, 0.0f};
    glm::vec2 mouse_norm_render{0.0f, 0.0f};
    glm::vec2 mouse_render_pos{0.0f, 0.0f};
    bool has_mouse_render_pos{false};
    std::vector<ControllerState> controllers;
};
