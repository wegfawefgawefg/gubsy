#pragma once

struct SDL_Renderer;
struct EngineState;

enum class InputSourceType {
    Keyboard,
    Mouse,
    Gamepad
};

struct DeviceIdentifier {
    int id{0};
};

struct InputSource {
    InputSourceType type{};
    DeviceIdentifier device_id{};
};

bool detect_input_sources(EngineState& engine);
void refresh_input_sources(EngineState& engine);
void on_device_added(EngineState& engine, int device_index);
void on_device_removed(EngineState& engine, int instance_id);
void draw_input_devices_overlay(const EngineState& engine, SDL_Renderer* renderer);
