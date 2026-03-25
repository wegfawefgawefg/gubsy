#include "game/coop_correction.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "game/state.hpp"

namespace {

constexpr float kCorrectionBlendPerSecond = 12.0f;
constexpr float kCorrectionSnapDistance = 3.0f;

struct CapturedVisual {
    std::string member_id;
    glm::vec2 render_pos{0.0f, 0.0f};
};

std::vector<CapturedVisual> g_captured_visuals;

const CapturedVisual* find_captured_visual(const std::string& member_id) {
    for (const CapturedVisual& captured : g_captured_visuals) {
        if (captured.member_id == member_id)
            return &captured;
    }
    return nullptr;
}

bool is_local_member(const std::vector<std::string>& member_ids,
                     std::size_t index,
                     const std::string& local_member_id) {
    return index < member_ids.size() && member_ids[index] == local_member_id;
}

} // namespace

void demo_sync_correction_reset() {
    g_captured_visuals.clear();
}

void demo_sync_correction_begin(const State& state, const std::vector<std::string>& member_ids) {
    g_captured_visuals.clear();
    const std::size_t count = std::min(state.players.size(), member_ids.size());
    g_captured_visuals.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        g_captured_visuals.push_back({
            member_ids[i],
            state.players[i].render_pos,
        });
    }
}

void demo_sync_correction_finish(State& state,
                                 const std::vector<std::string>& member_ids,
                                 const std::string& local_member_id,
                                 bool is_host) {
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        DemoPlayer& player = state.players[i];
        if (is_host) {
            player.render_pos = player.pos;
            continue;
        }
        if (is_local_member(member_ids, i, local_member_id)) {
            player.render_pos = player.pos;
            continue;
        }

        const CapturedVisual* captured =
            (i < member_ids.size()) ? find_captured_visual(member_ids[i]) : nullptr;
        if (!captured) {
            player.render_pos = player.pos;
            continue;
        }

        if (glm::length(player.pos - captured->render_pos) > kCorrectionSnapDistance) {
            player.render_pos = player.pos;
            continue;
        }

        player.render_pos = captured->render_pos;
    }
    g_captured_visuals.clear();
}

void demo_sync_correction_tick(State& state,
                               const std::vector<std::string>& member_ids,
                               const std::string& local_member_id,
                               bool is_host,
                               float dt) {
    const float blend = std::clamp(kCorrectionBlendPerSecond * dt, 0.0f, 1.0f);
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        DemoPlayer& player = state.players[i];
        if (is_host) {
            player.render_pos = player.pos;
            continue;
        }
        if (is_local_member(member_ids, i, local_member_id)) {
            player.render_pos = player.pos;
            continue;
        }

        const glm::vec2 delta = player.pos - player.render_pos;
        if (glm::length(delta) > kCorrectionSnapDistance) {
            player.render_pos = player.pos;
            continue;
        }

        player.render_pos += delta * blend;
    }
}
