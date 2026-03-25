#include "game/input_frame.hpp"

#include "engine/binds_profiles.hpp"
#include "engine/device_state.hpp"
#include "engine/globals.hpp"
#include "engine/input.hpp"
#include "engine/input_binding_utils.hpp"
#include "engine/player.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>

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

void build_input_frame(int player_index, const DeviceState& device_state, InputFrame& out) {
    out = InputFrame{};

    const BindsProfile* binds = get_player_binds_profile(player_index);
    if (!binds)
        return;

    for (const auto& [device_button, action] : binds->button_binds) {
        if (action < 0 || action >= 32)
            continue;
        if (device_button_is_down(device_state, device_button))
            out.down_bits |= (1u << action);
    }

    std::array<float, GameAnalog1D::COUNT> analog_1d_values{};
    std::array<float, GameAnalog1D::COUNT> analog_1d_mags{};
    for (const auto& [device_axis, analog_id] : binds->analog_1d_binds) {
        if (analog_id < 0 || static_cast<std::size_t>(analog_id) >= analog_1d_values.size())
            continue;
        float value = sample_analog_1d(device_state, device_axis);
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
    for (const auto& [device_axis, analog_id] : binds->analog_2d_binds) {
        if (analog_id < 0 || static_cast<std::size_t>(analog_id) >= analog_2d_values.size())
            continue;
        glm::vec2 value = sample_analog_2d(device_state, device_axis);
        float mag = glm::length(value);
        if (mag >= analog_2d_mags[static_cast<std::size_t>(analog_id)]) {
            analog_2d_values[static_cast<std::size_t>(analog_id)] = value;
            analog_2d_mags[static_cast<std::size_t>(analog_id)] = mag;
        }
    }
    for (std::size_t i = 0; i < analog_2d_values.size(); ++i)
        write_2d(out, static_cast<int>(i), analog_2d_values[i].x, analog_2d_values[i].y);
}
