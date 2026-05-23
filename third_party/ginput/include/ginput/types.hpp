#pragma once

#include <cstdint>

namespace ginput {

using ActionId = int;
using Axis1DId = int;
using Axis2DId = int;
using EncodedControl = int;

inline constexpr int any_device_id = -1;

enum class DeviceKind : std::uint8_t {
    Keyboard = 0,
    Mouse = 1,
    Gamepad = 2,
    Other = 3,
};

struct DeviceButton {
    DeviceKind kind = DeviceKind::Keyboard;
    int device_id = any_device_id;
    int code = 0;
};

struct DeviceAxis1D {
    DeviceKind kind = DeviceKind::Gamepad;
    int device_id = any_device_id;
    int code = 0;
};

struct DeviceAxis2D {
    DeviceKind kind = DeviceKind::Gamepad;
    int device_id = any_device_id;
    int x_code = 0;
    int y_code = 1;
};

EncodedControl encode_button(DeviceButton button);
EncodedControl encode_axis_1d(DeviceAxis1D axis);
EncodedControl encode_axis_2d(DeviceAxis2D axis);

bool decode_button(EncodedControl encoded, DeviceButton& out);
bool decode_axis_1d(EncodedControl encoded, DeviceAxis1D& out);
bool decode_axis_2d(EncodedControl encoded, DeviceAxis2D& out);

} // namespace ginput
