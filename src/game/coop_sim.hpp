#pragma once

#include <cstddef>
#include <vector>

#include "game/input_frame.hpp"
#include "game/state.hpp"

struct CoopSimEvents {
    int bonk_count{0};
};

void ensure_demo_player_count(State& state, std::size_t count);
void apply_demo_view_input(State& state, const InputFrame& frame);
CoopSimEvents simulate_demo_world(State& state,
                                  const std::vector<InputFrame>& current,
                                  const std::vector<InputFrame>& previous,
                                  float dt);
