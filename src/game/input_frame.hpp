#pragma once

#include <array>
#include <cstdint>

#include "game/actions.hpp"

struct DeviceState;

struct InputFrame {
    uint32_t down_bits{0};
    std::array<int16_t, GameAnalog1D::COUNT> analog_1d{};
    struct Analog2D {
        int16_t x{0};
        int16_t y{0};
    };
    std::array<Analog2D, GameAnalog2D::COUNT> analog_2d{};
};

void build_input_frame(int player_index, const DeviceState& device_state, InputFrame& out);
