#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "demo/input_frame.hpp"
#include "demo/state.hpp"

struct CoopPlayerSnapshot {
    std::string member_id;
    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 half_size{PLAYER_HALF_SIZE_UNITS, PLAYER_HALF_SIZE_UNITS};
    float speed_units_per_sec{PLAYER_MOVE_SPEED_UNITS};
};

struct CoopStateSnapshot {
    std::uint64_t sim_frame{0};
    std::vector<CoopPlayerSnapshot> players;
    BonkTarget bonk{};
    std::uint64_t bonk_serial{0};
    float bar_height{0.5f};
};

nlohmann::json input_frame_to_json(const InputFrame& frame);
bool input_frame_from_json(const nlohmann::json& json, InputFrame& frame);

nlohmann::json coop_snapshot_to_json(const CoopStateSnapshot& snapshot);
bool coop_snapshot_from_json(const nlohmann::json& json, CoopStateSnapshot& snapshot);

CoopStateSnapshot capture_coop_snapshot(const State& state, const std::vector<std::string>& member_ids, std::uint64_t sim_frame);
void apply_coop_snapshot(const CoopStateSnapshot& snapshot, State& state, std::vector<std::string>& member_ids_out);
