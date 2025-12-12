#include "state.hpp"
#include "globals.hpp"

bool init_state() {
    if (!ss)
        ss = new State{};
    ss->mode = modes::TITLE;
    ss->alerts.clear();
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
