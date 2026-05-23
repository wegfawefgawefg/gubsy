#pragma once

#include "ginput/button_state.hpp"
#include "ginput/profile.hpp"
#include "ginput/types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ginput {

float choose_larger_magnitude(float current, float candidate);
Vec2 choose_larger_magnitude(Vec2 current, Vec2 candidate);

enum class FrameReset {
    ClearCurrent,
    KeepCurrent,
};

class FrameState {
  public:
    void resize_actions(std::size_t count) {
        current_actions.resize(count, false);
        previous_actions.resize(count, false);
    }

    void resize_axes_1d(std::size_t count) {
        current_axes_1d.resize(count, 0.0f);
        previous_axes_1d.resize(count, 0.0f);
    }

    void resize_axes_2d(std::size_t count) {
        current_axes_2d.resize(count, Vec2{});
        previous_axes_2d.resize(count, Vec2{});
    }

    void begin_frame(FrameReset reset = FrameReset::ClearCurrent) {
        previous_actions = current_actions;
        previous_axes_1d = current_axes_1d;
        previous_axes_2d = current_axes_2d;
        if (reset == FrameReset::ClearCurrent) {
            std::fill(current_actions.begin(), current_actions.end(), false);
            std::fill(current_axes_1d.begin(), current_axes_1d.end(), 0.0f);
            std::fill(current_axes_2d.begin(), current_axes_2d.end(), Vec2{});
        }
    }

    void clear_current() {
        std::fill(current_actions.begin(), current_actions.end(), false);
        std::fill(current_axes_1d.begin(), current_axes_1d.end(), 0.0f);
        std::fill(current_axes_2d.begin(), current_axes_2d.end(), Vec2{});
    }

    void set_down(ActionId action, bool down) {
        if (!valid_index(action, current_actions.size())) {
            return;
        }
        current_actions[static_cast<std::size_t>(action)] = down;
    }

    bool down(ActionId action) const {
        if (!valid_index(action, current_actions.size())) {
            return false;
        }
        return current_actions[static_cast<std::size_t>(action)];
    }

    bool previous_down(ActionId action) const {
        if (!valid_index(action, previous_actions.size())) {
            return false;
        }
        return previous_actions[static_cast<std::size_t>(action)];
    }

    ButtonState button(ActionId action) const {
        return make_button_state(down(action), previous_down(action));
    }

    bool pressed(ActionId action) const {
        return button(action).pressed;
    }

    bool released(ActionId action) const {
        return button(action).released;
    }

    void set_axis_1d(Axis1DId axis, float value) {
        if (!valid_index(axis, current_axes_1d.size())) {
            return;
        }
        current_axes_1d[static_cast<std::size_t>(axis)] = std::clamp(value, -1.0f, 1.0f);
    }

    void merge_axis_1d(Axis1DId axis, float value) {
        if (!valid_index(axis, current_axes_1d.size())) {
            return;
        }
        float& current = current_axes_1d[static_cast<std::size_t>(axis)];
        current = choose_larger_magnitude(current, value);
    }

    float axis_1d(Axis1DId axis) const {
        if (!valid_index(axis, current_axes_1d.size())) {
            return 0.0f;
        }
        return current_axes_1d[static_cast<std::size_t>(axis)];
    }

    float previous_axis_1d(Axis1DId axis) const {
        if (!valid_index(axis, previous_axes_1d.size())) {
            return 0.0f;
        }
        return previous_axes_1d[static_cast<std::size_t>(axis)];
    }

    float axis_1d_delta(Axis1DId axis) const {
        return axis_1d(axis) - previous_axis_1d(axis);
    }

    void set_axis_2d(Axis2DId axis, Vec2 value) {
        if (!valid_index(axis, current_axes_2d.size())) {
            return;
        }
        current_axes_2d[static_cast<std::size_t>(axis)] = clamp_vec2(value);
    }

    void merge_axis_2d(Axis2DId axis, Vec2 value) {
        if (!valid_index(axis, current_axes_2d.size())) {
            return;
        }
        Vec2& current = current_axes_2d[static_cast<std::size_t>(axis)];
        current = choose_larger_magnitude(current, value);
    }

    Vec2 axis_2d(Axis2DId axis) const {
        if (!valid_index(axis, current_axes_2d.size())) {
            return Vec2{};
        }
        return current_axes_2d[static_cast<std::size_t>(axis)];
    }

    Vec2 previous_axis_2d(Axis2DId axis) const {
        if (!valid_index(axis, previous_axes_2d.size())) {
            return Vec2{};
        }
        return previous_axes_2d[static_cast<std::size_t>(axis)];
    }

    Vec2 axis_2d_delta(Axis2DId axis) const {
        const Vec2 current = axis_2d(axis);
        const Vec2 previous = previous_axis_2d(axis);
        return Vec2{current.x - previous.x, current.y - previous.y};
    }

  private:
    static bool valid_index(int index, std::size_t size) {
        return index >= 0 && static_cast<std::size_t>(index) < size;
    }

    static Vec2 clamp_vec2(Vec2 value) {
        return Vec2{
            std::clamp(value.x, -1.0f, 1.0f),
            std::clamp(value.y, -1.0f, 1.0f),
        };
    }

    std::vector<bool> current_actions;
    std::vector<bool> previous_actions;
    std::vector<float> current_axes_1d;
    std::vector<float> previous_axes_1d;
    std::vector<Vec2> current_axes_2d;
    std::vector<Vec2> previous_axes_2d;
};

struct RepeatConfig {
    float delay = 0.32f;
    float interval = 0.08f;
};

struct RepeatState {
    bool was_down = false;
    bool repeating = false;
    float timer = 0.0f;
};

struct RepeatResult {
    bool trigger = false;
    bool first_press = false;
    bool repeat = false;
};

inline void reset_repeat(RepeatState& state) {
    state = RepeatState{};
}

inline RepeatResult update_repeat(bool down, RepeatState& state, float dt,
                                  RepeatConfig config = RepeatConfig{}) {
    RepeatResult result;
    if (!down) {
        reset_repeat(state);
        return result;
    }

    if (!state.was_down) {
        state.was_down = true;
        state.repeating = false;
        state.timer = std::max(config.delay, 0.0f);
        result.trigger = true;
        result.first_press = true;
        return result;
    }

    state.timer -= std::max(dt, 0.0f);
    while (state.timer <= 0.0f) {
        result.trigger = true;
        result.repeat = true;
        state.repeating = true;
        state.timer += std::max(config.interval, 0.0001f);
    }
    return result;
}

class MouseWheelAccumulator {
  public:
    void add(float delta) {
        wheel_delta += delta;
    }

    float value() const {
        return wheel_delta;
    }

    float consume() {
        const float out = wheel_delta;
        wheel_delta = 0.0f;
        return out;
    }

    void clear() {
        wheel_delta = 0.0f;
    }

  private:
    float wheel_delta = 0.0f;
};

inline float choose_larger_magnitude(float current, float candidate) {
    const float clamped = std::clamp(candidate, -1.0f, 1.0f);
    if (std::fabs(clamped) >= std::fabs(current)) {
        return clamped;
    }
    return current;
}

inline Vec2 choose_larger_magnitude(Vec2 current, Vec2 candidate) {
    const Vec2 clamped{
        std::clamp(candidate.x, -1.0f, 1.0f),
        std::clamp(candidate.y, -1.0f, 1.0f),
    };
    const float current_len_sq = (current.x * current.x) + (current.y * current.y);
    const float candidate_len_sq = (clamped.x * clamped.x) + (clamped.y * clamped.y);
    if (candidate_len_sq >= current_len_sq) {
        return clamped;
    }
    return current;
}

} // namespace ginput
