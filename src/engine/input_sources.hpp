#pragma once

struct SDL_Renderer;

/*
 Detect all currently connected input devices and populate es->input_sources
 - Keyboard: always ID 0 (singleton, aggregates all keyboards)
 - Mouse: always ID 0 (singleton, aggregates all mice)
 - Gamepads: ID = SDL joystick index (0, 1, 2, ...)
 Returns true on success
*/
bool detect_input_sources();

/*
 Re-scan for input devices and update es->input_sources
 Call this periodically or in response to device add/remove events
 Detects new devices and removes disconnected ones
*/
void refresh_input_sources();

/*
 Handle SDL device connection event
 Called from SDL_CONTROLLERDEVICEADDED handler
*/
void on_device_added(int device_index);

/*
 Handle SDL device disconnection event
 Called from SDL_CONTROLLERDEVICEREMOVED handler
*/
void on_device_removed(int instance_id);

/*
 Draw input devices overlay
 Shows list of all detected input sources
*/
void draw_input_devices_overlay(SDL_Renderer* renderer);
