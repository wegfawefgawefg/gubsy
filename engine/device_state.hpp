#pragma once

#include <SDL2/SDL.h>
#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <vector>

struct DeviceState {
    struct ControllerState {
        int device_id{-1};
        std::array<float, SDL_CONTROLLER_AXIS_MAX> axes{};
        std::array<Uint8, SDL_CONTROLLER_BUTTON_MAX> buttons{};
    };

    std::array<Uint8, SDL_NUM_SCANCODES> keyboard{};
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
