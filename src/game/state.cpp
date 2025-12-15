#include "engine/globals.hpp"
#include "state.hpp"

bool init_state() {
    ss->player.pos = glm::vec2{0.0f, 0.0f};
    ss->bonk.pos = glm::vec2{2.5f, 0.0f};
    ss->bonk.cooldown = 0.0f;
    ss->bonk.enabled = true;
    return true;
}

void cleanup_state() {
    delete ss;
    ss = nullptr;
}
