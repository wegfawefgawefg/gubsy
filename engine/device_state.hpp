#pragma once

#include <SDL2/SDL.h>

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
    std::vector<ControllerState> controllers;
};
