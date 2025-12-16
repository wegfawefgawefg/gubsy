#include "engine/input_sources.hpp"

#include "engine/globals.hpp"
#include "engine/input.hpp"
#include "engine/render.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <string>

bool detect_input_sources() {
    if (!es)
        return false;

    es->input_sources.clear();

    // Always add keyboard (aggregates all keyboards, ID=0)
    InputSource keyboard;
    keyboard.type = InputSourceType::Keyboard;
    keyboard.device_id.id = 0;
    es->input_sources.push_back(keyboard);

    // Always add mouse (aggregates all mice, ID=0)
    InputSource mouse;
    mouse.type = InputSourceType::Mouse;
    mouse.device_id.id = 0;
    es->input_sources.push_back(mouse);

    // Enumerate gamepads
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            InputSource gamepad;
            gamepad.type = InputSourceType::Gamepad;
            gamepad.device_id.id = i;
            es->input_sources.push_back(gamepad);
        }
    }

    std::fprintf(stderr, "[input] Detected %zu input sources (%d gamepads)\n",
                 es->input_sources.size(), num_joysticks);

    return true;
}

void refresh_input_sources() {
    if (!es)
        return;

    // Keep keyboard and mouse (first 2 entries), only refresh gamepads
    size_t old_count = es->input_sources.size();

    // Remove all gamepads
    es->input_sources.erase(
        std::remove_if(es->input_sources.begin(), es->input_sources.end(),
                      [](const InputSource& src) { return src.type == InputSourceType::Gamepad; }),
        es->input_sources.end());

    // Re-enumerate gamepads
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            InputSource gamepad;
            gamepad.type = InputSourceType::Gamepad;
            gamepad.device_id.id = i;
            es->input_sources.push_back(gamepad);
        }
    }

    size_t new_count = es->input_sources.size();
    if (old_count != new_count) {
        std::fprintf(stderr, "[input] Input sources changed: %zu -> %zu\n", old_count, new_count);
    }
}

void on_device_added(int device_index) {
    if (!es)
        return;

    if (!SDL_IsGameController(device_index))
        return;

    // Check if already in list
    for (const auto& src : es->input_sources) {
        if (src.type == InputSourceType::Gamepad && src.device_id.id == device_index)
            return;
    }

    InputSource gamepad;
    gamepad.type = InputSourceType::Gamepad;
    gamepad.device_id.id = device_index;
    es->input_sources.push_back(gamepad);

    std::fprintf(stderr, "[input] Gamepad added (device %d)\n", device_index);
}

void on_device_removed(int instance_id) {
    if (!es)
        return;

    // Find and remove the gamepad with matching instance ID
    auto it = std::remove_if(es->input_sources.begin(), es->input_sources.end(),
                            [instance_id](const InputSource& src) {
                                return src.type == InputSourceType::Gamepad &&
                                       src.device_id.id == instance_id;
                            });

    if (it != es->input_sources.end()) {
        es->input_sources.erase(it, es->input_sources.end());
        std::fprintf(stderr, "[input] Gamepad removed (instance %d)\n", instance_id);
    }
}

void draw_input_devices_overlay(SDL_Renderer* renderer) {
    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color green = {100, 255, 100, 255};
    const SDL_Color cyan = {100, 200, 255, 255};

    int y = 10;
    const int line_height = 18;

    // Title
    std::string title = "Input Devices (" + std::to_string(es->input_sources.size()) + ")";
    draw_text(renderer, title, 10, y, white);
    y += line_height + 5;

    // List each device
    for (size_t i = 0; i < es->input_sources.size(); ++i) {
        const auto& src = es->input_sources[i];
        std::string device_text;

        switch (src.type) {
            case InputSourceType::Keyboard:
                device_text = "Keyboard (ID: " + std::to_string(src.device_id.id) + ")";
                draw_text(renderer, device_text, 20, y, green);
                break;
            case InputSourceType::Mouse:
                device_text = "Mouse (ID: " + std::to_string(src.device_id.id) + ")";
                draw_text(renderer, device_text, 20, y, green);
                break;
            case InputSourceType::Gamepad:
                device_text = "Gamepad (ID: " + std::to_string(src.device_id.id) + ")";
                draw_text(renderer, device_text, 20, y, cyan);
                break;
        }

        y += line_height;
    }
}
