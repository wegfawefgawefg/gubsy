#include "step.hpp"

#include "engine/audio.hpp"
#include "globals.hpp"
#include "settings.hpp"
#include "state.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <string>

namespace {
glm::vec2 movement_direction() {
    glm::vec2 dir{0.0f, 0.0f};
    if (ss->playing_inputs.left)
        dir.x -= 1.0f;
    if (ss->playing_inputs.right)
        dir.x += 1.0f;
    if (ss->playing_inputs.up)
        dir.y -= 1.0f;
    if (ss->playing_inputs.down)
        dir.y += 1.0f;
    if (glm::length(dir) > 0.01f)
        dir = glm::normalize(dir);
    return dir;
}

bool overlaps(const glm::vec2& a_pos, const glm::vec2& a_half,
              const glm::vec2& b_pos, const glm::vec2& b_half) {
    glm::vec2 delta = glm::abs(a_pos - b_pos);
    return delta.x <= (a_half.x + b_half.x) && delta.y <= (a_half.y + b_half.y);
}

void add_alert(const std::string& text) {
    if (!ss)
        return;
    State::Alert al;
    al.text = text;
    al.ttl = 1.2f;
    ss->alerts.push_back(al);
}
} // namespace

void step_playing() {
    if (!ss)
        return;

    const float dt = FIXED_TIMESTEP;
    auto& player = ss->player;
    auto& target = ss->bonk;

    glm::vec2 dir = movement_direction();
    player.pos += dir * player.speed_units_per_sec * dt;

    if (target.cooldown > 0.0f)
        target.cooldown = std::max(0.0f, target.cooldown - dt);

    if (overlaps(player.pos, player.half_size, target.pos, target.half_size) &&
        target.cooldown <= 0.0f) {
        target.cooldown = BONK_COOLDOWN_SECONDS;
        add_alert("bonk!");
        const std::string sound = target.sound_key.empty() ? "base:ui_confirm" : target.sound_key;
        play_sound(sound);
    }
}
