#include "game/input_runtime.hpp"

#include "engine/globals.hpp"
#include "game/input_frame.hpp"

#include <vector>

namespace {

std::vector<InputFrame> g_current_frames;
std::vector<InputFrame> g_previous_frames;

void ensure_input_frame_capacity() {
    const std::size_t count = es ? es->players.size() : 0;
    if (!es)
        return;
    if (g_current_frames.size() < count) {
        g_current_frames.resize(count);
        g_previous_frames.resize(count);
    }
}

const InputFrame& safe_frame(const std::vector<InputFrame>& frames, int player_index) {
    static InputFrame empty{};
    if (!es)
        return empty;
    if (player_index < 0 || static_cast<std::size_t>(player_index) >= frames.size())
        return empty;
    return frames[static_cast<std::size_t>(player_index)];
}

} // namespace

void build_input_frames_for_step() {
    if (!es)
        return;
    ensure_input_frame_capacity();
    for (std::size_t i = 0; i < es->players.size(); ++i) {
        g_previous_frames[i] = g_current_frames[i];
        InputFrame next{};
        build_input_frame(static_cast<int>(i), es->device_state, next);
        g_current_frames[i] = next;
    }
    es->device_state.mouse_wheel = 0;
    es->device_state.mouse_dx = 0;
    es->device_state.mouse_dy = 0;
}

const InputFrame& current_input_frame(int player_index) {
    return safe_frame(g_current_frames, player_index);
}

const InputFrame& previous_input_frame(int player_index) {
    return safe_frame(g_previous_frames, player_index);
}
