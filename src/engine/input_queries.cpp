#include "engine/input_queries.hpp"
#include "engine/globals.hpp"
#include "engine/input.hpp"
#include "game/input_frame.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/player.hpp"
#include "engine/user_profiles.hpp"
#include "engine/input_system.hpp"

bool is_down(int player_index, int game_button) {
    if (!es || game_button < 0 || game_button >= 32)
        return false;
    const InputFrame& frame = current_input_frame(player_index);
    return (frame.down_bits & (1u << game_button)) != 0;
}

bool was_pressed(int player_index, int game_button) {
    if (!es || game_button < 0 || game_button >= 32)
        return false;
    const InputFrame& cur = current_input_frame(player_index);
    const InputFrame& prev = previous_input_frame(player_index);
    const uint32_t mask = (1u << game_button);
    return (cur.down_bits & mask) && !(prev.down_bits & mask);
}

bool was_released(int player_index, int game_button) {
    if (!es || game_button < 0 || game_button >= 32)
        return false;
    const InputFrame& cur = current_input_frame(player_index);
    const InputFrame& prev = previous_input_frame(player_index);
    const uint32_t mask = (1u << game_button);
    return !(cur.down_bits & mask) && (prev.down_bits & mask);
}

float get_1d_analog(int player_index, int game_axis) {
    if (!es || game_axis < 0 || static_cast<std::size_t>(game_axis) >= GameAnalog1D::COUNT)
        return 0.0f;
    const InputFrame& frame = current_input_frame(player_index);
    return static_cast<float>(frame.analog_1d[static_cast<std::size_t>(game_axis)]) / 32767.0f;
}

float get_1d_analog_delta(int /*player_index*/, int /*game_axis*/) {
    // TODO: Not implemented. Requires historical state access.
    return 0.0f;
}

glm::vec2 get_2d_analog(int player_index, int game_axis) {
    if (!es || game_axis < 0 || static_cast<std::size_t>(game_axis) >= GameAnalog2D::COUNT)
        return glm::vec2(0.0f);
    const InputFrame& frame = current_input_frame(player_index);
    const auto& analog = frame.analog_2d[static_cast<std::size_t>(game_axis)];
    return glm::vec2(static_cast<float>(analog.x) / 32767.0f,
                     static_cast<float>(analog.y) / 32767.0f);
}

glm::vec2 get_2d_analog_delta(int /*player_index*/, int /*game_axis*/) {
    // TODO: Not implemented. Requires historical state access.
    return glm::vec2(0.0f);
}

glm::vec2 get_mouse_pos(int /*player_index*/) { return glm::vec2(0.0f); }
glm::vec2 get_mouse_delta(int /*player_index*/) { return glm::vec2(0.0f); }
glm::vec2 get_mouse_pos_window(int /*player_index*/) { return glm::vec2(0.0f); }
glm::vec2 get_mouse_pos_screen(int /*player_index*/) { return glm::vec2(0.0f); }
int get_mouse_wheel_delta(int /*player_index*/) { return 0; }
