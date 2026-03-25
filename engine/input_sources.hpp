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

/*
 Detect all currently connected input devices and populate engine.input_sources
 - Keyboard: always ID 0 (singleton, aggregates all keyboards)
 - Mouse: always ID 0 (singleton, aggregates all mice)
 - Gamepads: ID = SDL joystick index (0, 1, 2, ...)
 Returns true on success
*/
bool detect_input_sources(EngineState& engine);

/*
 Re-scan for input devices and update engine.input_sources
 Call this periodically or in response to device add/remove events
 Detects new devices and removes disconnected ones
*/
void refresh_input_sources(EngineState& engine);

/*
 Handle SDL device connection event
 Called from SDL_CONTROLLERDEVICEADDED handler
*/
void on_device_added(EngineState& engine, int device_index);

/*
 Handle SDL device disconnection event
 Called from SDL_CONTROLLERDEVICEREMOVED handler
*/
void on_device_removed(EngineState& engine, int instance_id);

/*
 Draw input devices overlay
 Shows list of all detected input sources
*/
void draw_input_devices_overlay(const EngineState& engine, SDL_Renderer* renderer);
