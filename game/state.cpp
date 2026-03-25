#include "state.hpp"

void reset_state(State& state) {
    state = State{};
    state.bonk.pos = glm::vec2{2.5f, 0.0f};
    state.bonk.cooldown = 0.0f;
    state.bonk.enabled = true;
}
