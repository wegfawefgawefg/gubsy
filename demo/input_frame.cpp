#include "demo/input_frame.hpp"

#include "src/binds_profiles.hpp"
#include "src/device_state.hpp"
#include "src/engine_state.hpp"
#include "src/input.hpp"
#include "src/input_binding_utils.hpp"
#include "src/player.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ginput/ginput.hpp>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>

namespace {

int16_t encode_axis(float value) {
    value = std::clamp(value, -1.0f, 1.0f);
    return static_cast<int16_t>(std::round(value * 32767.0f));
}

void write_1d(InputFrame& frame, int analog_index, float value) {
    frame.analog_1d[static_cast<std::size_t>(analog_index)] = encode_axis(value);
}

void write_2d(InputFrame& frame, int analog_index, float x, float y) {
    auto& dest = frame.analog_2d[static_cast<std::size_t>(analog_index)];
    dest.x = encode_axis(x);
    dest.y = encode_axis(y);
}

} // namespace

void build_input_frame(EngineState& engine, int player_index, const DeviceState& device_state,
                       InputFrame& out) {
    (void)device_state;
    out = InputFrame{};

    const BindsProfile* binds = get_player_binds_profile(engine, player_index);
    if (!binds)
        return;

    for (const auto& bind : binds->button_binds()) {
        int device_button = bind.device_button;
        int action = bind.action;
        if (action < 0 || action >= 32)
            continue;
        if (device_button_is_down(engine, device_button))
            out.down_bits |= (1u << action);
    }

    std::array<float, GameAnalog1D::COUNT> analog_1d_values{};
    std::array<float, GameAnalog1D::COUNT> analog_1d_mags{};
    for (const auto& bind : binds->axis_1d_binds()) {
        int device_axis = bind.device_axis;
        int analog_id = bind.axis_1d;
        if (analog_id < 0 || static_cast<std::size_t>(analog_id) >= analog_1d_values.size())
            continue;
        float value = ginput::apply_axis_transform(sample_analog_1d(engine, device_axis),
                                                   bind.scale, bind.deadzone);
        float mag = std::fabs(value);
        if (mag >= analog_1d_mags[static_cast<std::size_t>(analog_id)]) {
            analog_1d_values[static_cast<std::size_t>(analog_id)] = value;
            analog_1d_mags[static_cast<std::size_t>(analog_id)] = mag;
        }
    }
    for (std::size_t i = 0; i < analog_1d_values.size(); ++i)
        write_1d(out, static_cast<int>(i), analog_1d_values[i]);

    std::array<glm::vec2, GameAnalog2D::COUNT> analog_2d_values{};
    std::array<float, GameAnalog2D::COUNT> analog_2d_mags{};
    for (const auto& bind : binds->axis_2d_binds()) {
        int device_axis = bind.device_stick;
        int analog_id = bind.axis_2d;
        if (analog_id < 0 || static_cast<std::size_t>(analog_id) >= analog_2d_values.size())
            continue;
        glm::vec2 value = sample_analog_2d(engine, device_axis);
        ginput::Vec2 transformed = ginput::apply_stick_transform(
            ginput::Vec2{value.x, value.y}, bind.scale_x, bind.scale_y, bind.deadzone);
        value = glm::vec2(transformed.x, transformed.y);
        float mag = glm::length(value);
        if (mag >= analog_2d_mags[static_cast<std::size_t>(analog_id)]) {
            analog_2d_values[static_cast<std::size_t>(analog_id)] = value;
            analog_2d_mags[static_cast<std::size_t>(analog_id)] = mag;
        }
    }
    for (std::size_t i = 0; i < analog_2d_values.size(); ++i)
        write_2d(out, static_cast<int>(i), analog_2d_values[i].x, analog_2d_values[i].y);
}
