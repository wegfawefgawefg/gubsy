#pragma once

#include "gubsy/input/sources.hpp"
#include "src/device_state.hpp"
#include "src/input.hpp"

#include <glm/glm.hpp>

struct EngineState;

enum class DeviceInputKind : uint8_t { Keyboard = 0, Mouse = 1, Gamepad = 2 };

inline constexpr int kAnyDeviceId = -1;

// Encoded binding helpers ----------------------------------------------------

int encode_device_button(DeviceInputKind kind, int device_id, int code);
int encode_device_analog_1d(DeviceInputKind kind, int device_id, int axis_code);
int encode_device_analog_2d(DeviceInputKind kind, int device_id, int axis_x_code, int axis_y_code);

// Sampling helpers -----------------------------------------------------------

bool device_button_is_down(const EngineState& engine, int encoded_button);
bool device_button_is_down_for_source(const EngineState& engine, int encoded_button,
                                      InputSourceType source_type, int source_id);
float sample_analog_1d(const EngineState& engine, int encoded_axis);
float sample_analog_1d_for_source(const EngineState& engine, int encoded_axis,
                                  InputSourceType source_type, int source_id);
glm::vec2 sample_analog_2d(const EngineState& engine, int encoded_axis);
glm::vec2 normalized_mouse_coords(const DeviceState& state);
glm::vec2 normalized_mouse_coords_in_render(const DeviceState& state);
bool mouse_render_position(const EngineState& engine, float render_width, float render_height,
                           float& out_x, float& out_y);
