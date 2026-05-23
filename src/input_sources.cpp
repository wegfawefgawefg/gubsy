#include "src/input_sources.hpp"

#include "src/engine_state.hpp"
#include "src/input.hpp"
#include "src/render.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <string>

bool detect_input_sources(EngineState& engine) {
    engine.input_sources.clear();
    engine.open_controllers.clear();
    engine.gamepad_states.clear();

    // Always add keyboard (aggregates all keyboards, ID=0)
    InputSource keyboard;
    keyboard.type = InputSourceType::Keyboard;
    keyboard.device_id.id = 0;
    engine.input_sources.push_back(keyboard);

    // Always add mouse (aggregates all mice, ID=0)
    InputSource mouse;
    mouse.type = InputSourceType::Mouse;
    mouse.device_id.id = 0;
    engine.input_sources.push_back(mouse);

    // Enumerate and open gamepads
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            on_device_added(engine, i);
        }
    }

    std::fprintf(stderr, "[input] Detected %zu input sources (%zu gamepads)\n",
                 engine.input_sources.size(), engine.open_controllers.size());

    return true;
}

void refresh_input_sources(EngineState& engine) {
    size_t old_count = engine.open_controllers.size();

    // Close all currently open game controllers
    for (auto const& [id, controller] : engine.open_controllers) {
        SDL_GameControllerClose(controller);
    }
    engine.open_controllers.clear();
    engine.gamepad_states.clear();

    // Remove all gamepads from the public list
    engine.input_sources.erase(
        std::remove_if(engine.input_sources.begin(), engine.input_sources.end(),
                      [](const InputSource& src) { return src.type == InputSourceType::Gamepad; }),
        engine.input_sources.end());

    // Re-enumerate and open gamepads
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i) {
        if (SDL_IsGameController(i)) {
            on_device_added(engine, i);
        }
    }

    size_t new_count = engine.open_controllers.size();
    if (old_count != new_count) {
        std::fprintf(stderr, "[input] Input sources refreshed: %zu -> %zu gamepads\n", old_count, new_count);
    }
}

void on_device_added(EngineState& engine, int device_index) {
    if (!SDL_IsGameController(device_index))
        return;

    // Check if already open
    if (engine.open_controllers.count(device_index)) {
        return;
    }
    
    SDL_GameController* controller = SDL_GameControllerOpen(device_index);
    if (!controller) {
        std::fprintf(stderr, "[input] Could not open gamecontroller %i: %s\n", device_index, SDL_GetError());
        return;
    }

    engine.open_controllers[device_index] = controller;
    engine.gamepad_states[device_index] = {};

    // Also add to the public list of sources
    InputSource gamepad;
    gamepad.type = InputSourceType::Gamepad;
    gamepad.device_id.id = device_index;
    engine.input_sources.push_back(gamepad);

    std::fprintf(stderr, "[input] Gamepad added and opened (device %d)\n", device_index);
}

void on_device_removed(EngineState& engine, int instance_id) {
    int device_to_remove = -1;
    SDL_GameController* controller_to_close = nullptr;

    // Find the controller and its device_id from the instance_id
    for (auto const& [device_id, controller] : engine.open_controllers) {
        if (static_cast<int>(SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) == instance_id) {
            device_to_remove = device_id;
            controller_to_close = controller;
            break;
        }
    }

    if (controller_to_close) {
        std::fprintf(stderr, "[input] Gamepad removed (device %d, instance %d)\n", device_to_remove, instance_id);
        
        // Close handle and remove from maps
        SDL_GameControllerClose(controller_to_close);
        engine.open_controllers.erase(device_to_remove);
        engine.gamepad_states.erase(device_to_remove);

        // Remove from public list of sources
        engine.input_sources.erase(
            std::remove_if(engine.input_sources.begin(), engine.input_sources.end(),
                          [device_to_remove](const InputSource& src) {
                              return src.type == InputSourceType::Gamepad && src.device_id.id == device_to_remove;
                          }),
            engine.input_sources.end());
    }
}

void draw_input_devices_overlay(const EngineState& engine, SDL_Renderer* renderer) {
    const SDL_Color white = {255, 255, 255, 255};
    const SDL_Color green = {100, 255, 100, 255};
    const SDL_Color cyan = {100, 200, 255, 255};

    int y = 10;
    const int line_height = 18;

    // Title
    std::string title = "Input Devices (" + std::to_string(engine.input_sources.size()) + ")";
    draw_text(renderer, title, 10, y, white);
    y += line_height + 5;

    // List each device
    for (size_t i = 0; i < engine.input_sources.size(); ++i) {
        const auto& src = engine.input_sources[i];
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
