#pragma once

#include <glm/glm.hpp>

#include "engine/device_state.hpp"
#include "engine/input.hpp"

enum class DeviceInputKind : uint8_t {
    Keyboard = 0,
    Mouse = 1,
    Gamepad = 2
};

inline constexpr int kAnyDeviceId = -1;

// Encoded binding helpers ----------------------------------------------------

int encode_device_button(DeviceInputKind kind, int device_id, int code);
int encode_device_analog_1d(DeviceInputKind kind, int device_id, int axis_code);
int encode_device_analog_2d(DeviceInputKind kind, int device_id, int axis_x_code, int axis_y_code);

// Sampling helpers -----------------------------------------------------------

bool device_button_is_down(const DeviceState& state, int encoded_button);
float sample_analog_1d(const DeviceState& state, int encoded_axis);
glm::vec2 sample_analog_2d(const DeviceState& state, int encoded_axis);
glm::vec2 normalized_mouse_coords(const DeviceState& state);
glm::vec2 normalized_mouse_coords_in_render(const DeviceState& state);
bool mouse_render_position(const DeviceState& state,
                           float render_width,
                           float render_height,
                           float& out_x,
                           float& out_y);
