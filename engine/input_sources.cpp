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
    es->open_controllers.clear();
    es->gamepad_states.clear();

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

    // Enumerate and open gamepads
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            on_device_added(i); // Use the same logic as hot-plugging
        }
    }

    std::fprintf(stderr, "[input] Detected %zu input sources (%zu gamepads)\n",
                 es->input_sources.size(), es->open_controllers.size());

    return true;
}

void refresh_input_sources() {
    if (!es)
        return;

    size_t old_count = es->open_controllers.size();

    // Close all currently open game controllers
    for (auto const& [id, controller] : es->open_controllers) {
        SDL_GameControllerClose(controller);
    }
    es->open_controllers.clear();
    es->gamepad_states.clear();

    // Remove all gamepads from the public list
    es->input_sources.erase(
        std::remove_if(es->input_sources.begin(), es->input_sources.end(),
                      [](const InputSource& src) { return src.type == InputSourceType::Gamepad; }),
        es->input_sources.end());

    // Re-enumerate and open gamepads
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            on_device_added(i); // Reuse hot-plug logic
        }
    }

    size_t new_count = es->open_controllers.size();
    if (old_count != new_count) {
        std::fprintf(stderr, "[input] Input sources refreshed: %zu -> %zu gamepads\n", old_count, new_count);
    }
}

void on_device_added(int device_index) {
    if (!es)
        return;

    if (!SDL_IsGameController(device_index))
        return;

    // Check if already open
    if (es->open_controllers.count(device_index)) {
        return;
    }
    
    SDL_GameController* controller = SDL_GameControllerOpen(device_index);
    if (!controller) {
        std::fprintf(stderr, "[input] Could not open gamecontroller %i: %s\n", device_index, SDL_GetError());
        return;
    }

    es->open_controllers[device_index] = controller;
    es->gamepad_states[device_index] = {}; // Zero-initialize the state

    // Also add to the public list of sources
    InputSource gamepad;
    gamepad.type = InputSourceType::Gamepad;
    gamepad.device_id.id = device_index;
    es->input_sources.push_back(gamepad);

    std::fprintf(stderr, "[input] Gamepad added and opened (device %d)\n", device_index);
}

void on_device_removed(int instance_id) {
    if (!es)
        return;

    int device_to_remove = -1;
    SDL_GameController* controller_to_close = nullptr;

    // Find the controller and its device_id from the instance_id
    for (auto const& [device_id, controller] : es->open_controllers) {
        if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == instance_id) {
            device_to_remove = device_id;
            controller_to_close = controller;
            break;
        }
    }

    if (controller_to_close) {
        std::fprintf(stderr, "[input] Gamepad removed (device %d, instance %d)\n", device_to_remove, instance_id);
        
        // Close handle and remove from maps
        SDL_GameControllerClose(controller_to_close);
        es->open_controllers.erase(device_to_remove);
        es->gamepad_states.erase(device_to_remove);

        // Remove from public list of sources
        es->input_sources.erase(
            std::remove_if(es->input_sources.begin(), es->input_sources.end(),
                          [device_to_remove](const InputSource& src) {
                              return src.type == InputSourceType::Gamepad && src.device_id.id == device_to_remove;
                          }),
            es->input_sources.end());
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
