#include "demo/coop_sim.hpp"

#include <algorithm>
#include <glm/geometric.hpp>

#include "demo/actions.hpp"
#include "demo/settings.hpp"

namespace {

bool input_is_down(const InputFrame& frame, int action) {
    if (action < 0 || action >= 32)
        return false;
    return (frame.down_bits & (1u << action)) != 0;
}

bool input_was_pressed(const InputFrame& current, const InputFrame& previous, int action) {
    return input_is_down(current, action) && !input_is_down(previous, action);
}

float decode_axis_1d(int16_t value) {
    return static_cast<float>(value) / 32767.0f;
}

glm::vec2 decode_axis_2d(const InputFrame::Analog2D& value) {
    return glm::vec2(static_cast<float>(value.x) / 32767.0f,
                     static_cast<float>(value.y) / 32767.0f);
}

bool overlaps(const glm::vec2& a_pos, const glm::vec2& a_half,
              const glm::vec2& b_pos, const glm::vec2& b_half) {
    glm::vec2 delta = glm::abs(a_pos - b_pos);
    return delta.x <= (a_half.x + b_half.x) && delta.y <= (a_half.y + b_half.y);
}

const InputFrame& frame_or_empty(const std::vector<InputFrame>& frames, std::size_t index) {
    static const InputFrame empty{};
    if (index >= frames.size())
        return empty;
    return frames[index];
}

} // namespace

void ensure_demo_player_count(State& state, std::size_t count) {
    if (count == 0)
        count = 1;
    if (state.players.size() < count) {
        std::size_t old_size = state.players.size();
        state.players.resize(count);
        for (std::size_t i = old_size; i < count; ++i) {
            DemoPlayer& player = state.players[i];
            player.pos = glm::vec2(static_cast<float>(i) * 1.5f, 0.0f);
            player.render_pos = player.pos;
        }
    } else if (state.players.size() > count) {
        state.players.resize(count);
    }
}

void apply_demo_view_input(State& state, const InputFrame& frame) {
    state.bar_height = std::clamp(decode_axis_1d(frame.analog_1d[0]), 0.0f, 1.0f);
    state.reticle_pos = glm::clamp(decode_axis_2d(frame.analog_2d[0]), glm::vec2(-1.0f), glm::vec2(1.0f));
    state.reticle_pos_gamepad = state.reticle_pos;
    state.reticle_pos_mouse = state.reticle_pos;
}

CoopSimEvents simulate_demo_world(State& state,
                                  const std::vector<InputFrame>& current,
                                  const std::vector<InputFrame>& previous,
                                  float dt) {
    CoopSimEvents events;
    ensure_demo_player_count(state, std::max<std::size_t>(1, current.size()));

    if (state.bonk.cooldown > 0.0f)
        state.bonk.cooldown = std::max(0.0f, state.bonk.cooldown - dt);

    const InputFrame& player0 = frame_or_empty(current, 0);
    apply_demo_view_input(state, player0);

    for (std::size_t i = 0; i < state.players.size(); ++i) {
        DemoPlayer& player = state.players[i];
        const InputFrame& cur = frame_or_empty(current, i);
        const InputFrame& prev = frame_or_empty(previous, i);

        glm::vec2 dir(0.0f);
        if (input_is_down(cur, GameAction::UP))
            dir.y -= 1.0f;
        if (input_is_down(cur, GameAction::DOWN))
            dir.y += 1.0f;
        if (input_is_down(cur, GameAction::LEFT))
            dir.x -= 1.0f;
        if (input_is_down(cur, GameAction::RIGHT))
            dir.x += 1.0f;
        if (glm::length(dir) > 0.0f)
            dir = glm::normalize(dir);
        player.pos += dir * player.speed_units_per_sec * dt;

        if (state.bonk.enabled &&
            overlaps(player.pos, player.half_size, state.bonk.pos, state.bonk.half_size) &&
            state.bonk.cooldown <= 0.0f &&
            input_was_pressed(cur, prev, GameAction::USE)) {
            state.bonk.cooldown = BONK_COOLDOWN_SECONDS;
            state.bonk_serial += 1;
            events.bonk_count += 1;
        }
    }

    return events;
}
