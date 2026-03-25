#include "game/input_runtime.hpp"

#include "game/input_frame.hpp"

#include <vector>

namespace {

std::vector<InputFrame> g_current_frames;
std::vector<InputFrame> g_previous_frames;

void ensure_input_frame_capacity(const EngineState& engine) {
    const std::size_t count = engine.players.size();
    if (g_current_frames.size() < count) {
        g_current_frames.resize(count);
        g_previous_frames.resize(count);
    }
}

const InputFrame& safe_frame(const std::vector<InputFrame>& frames, int player_index) {
    static InputFrame empty{};
    if (player_index < 0 || static_cast<std::size_t>(player_index) >= frames.size())
        return empty;
    return frames[static_cast<std::size_t>(player_index)];
}

} // namespace

void build_input_frames_for_step(EngineState& engine, void*) {
    ensure_input_frame_capacity(engine);
    for (std::size_t i = 0; i < engine.players.size(); ++i) {
        g_previous_frames[i] = g_current_frames[i];
        InputFrame next{};
        build_input_frame(engine, static_cast<int>(i), engine.device_state, next);
        g_current_frames[i] = next;
    }
    engine.device_state.mouse_wheel = 0;
    engine.device_state.mouse_dx = 0;
    engine.device_state.mouse_dy = 0;
}

const InputFrame& current_input_frame(int player_index) {
    return safe_frame(g_current_frames, player_index);
}

const InputFrame& previous_input_frame(int player_index) {
    return safe_frame(g_previous_frames, player_index);
}
