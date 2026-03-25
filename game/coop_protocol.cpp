#include "game/coop_protocol.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace {

nlohmann::json vec2_to_json(const glm::vec2& value) {
    return nlohmann::json::array({value.x, value.y});
}

bool vec2_from_json(const nlohmann::json& json, glm::vec2& out) {
    if (!json.is_array() || json.size() != 2)
        return false;
    if (!json[0].is_number() || !json[1].is_number())
        return false;
    out.x = json[0].get<float>();
    out.y = json[1].get<float>();
    return true;
}

} // namespace

nlohmann::json input_frame_to_json(const InputFrame& frame) {
    nlohmann::json analog_1d = nlohmann::json::array();
    for (int16_t value : frame.analog_1d)
        analog_1d.push_back(value);

    nlohmann::json analog_2d = nlohmann::json::array();
    for (const auto& value : frame.analog_2d) {
        analog_2d.push_back({
            {"x", value.x},
            {"y", value.y},
        });
    }

    return {
        {"down_bits", frame.down_bits},
        {"analog_1d", std::move(analog_1d)},
        {"analog_2d", std::move(analog_2d)},
    };
}

bool input_frame_from_json(const nlohmann::json& json, InputFrame& frame) {
    if (!json.is_object())
        return false;
    frame = InputFrame{};
    frame.down_bits = json.value("down_bits", 0u);

    auto analog_1d_it = json.find("analog_1d");
    if (analog_1d_it != json.end() && analog_1d_it->is_array()) {
        std::size_t count = std::min(frame.analog_1d.size(), analog_1d_it->size());
        for (std::size_t i = 0; i < count; ++i)
            frame.analog_1d[i] = (*analog_1d_it)[i].get<int16_t>();
    }

    auto analog_2d_it = json.find("analog_2d");
    if (analog_2d_it != json.end() && analog_2d_it->is_array()) {
        std::size_t count = std::min(frame.analog_2d.size(), analog_2d_it->size());
        for (std::size_t i = 0; i < count; ++i) {
            const auto& entry = (*analog_2d_it)[i];
            frame.analog_2d[i].x = entry.value("x", int16_t{0});
            frame.analog_2d[i].y = entry.value("y", int16_t{0});
        }
    }
    return true;
}

nlohmann::json coop_snapshot_to_json(const CoopStateSnapshot& snapshot) {
    nlohmann::json players = nlohmann::json::array();
    for (const auto& player : snapshot.players) {
        players.push_back({
            {"member_id", player.member_id},
            {"pos", vec2_to_json(player.pos)},
            {"half_size", vec2_to_json(player.half_size)},
            {"speed_units_per_sec", player.speed_units_per_sec},
        });
    }

    return {
        {"sim_frame", snapshot.sim_frame},
        {"players", std::move(players)},
        {"bonk",
         {
             {"pos", vec2_to_json(snapshot.bonk.pos)},
             {"half_size", vec2_to_json(snapshot.bonk.half_size)},
             {"cooldown", snapshot.bonk.cooldown},
             {"sound_key", snapshot.bonk.sound_key},
             {"enabled", snapshot.bonk.enabled},
         }},
        {"bonk_serial", snapshot.bonk_serial},
        {"bar_height", snapshot.bar_height},
    };
}

bool coop_snapshot_from_json(const nlohmann::json& json, CoopStateSnapshot& snapshot) {
    if (!json.is_object())
        return false;
    snapshot = CoopStateSnapshot{};
    snapshot.sim_frame = json.value("sim_frame", std::uint64_t{0});
    snapshot.bonk_serial = json.value("bonk_serial", std::uint64_t{0});
    snapshot.bar_height = json.value("bar_height", 0.5f);

    auto bonk_it = json.find("bonk");
    if (bonk_it != json.end() && bonk_it->is_object()) {
        vec2_from_json((*bonk_it)["pos"], snapshot.bonk.pos);
        vec2_from_json((*bonk_it)["half_size"], snapshot.bonk.half_size);
        snapshot.bonk.cooldown = bonk_it->value("cooldown", 0.0f);
        snapshot.bonk.sound_key = bonk_it->value("sound_key", std::string{"base:ui_confirm"});
        snapshot.bonk.enabled = bonk_it->value("enabled", true);
    }

    auto players_it = json.find("players");
    if (players_it != json.end() && players_it->is_array()) {
        for (const auto& entry : *players_it) {
            if (!entry.is_object())
                continue;
            CoopPlayerSnapshot player;
            player.member_id = entry.value("member_id", "");
            vec2_from_json(entry["pos"], player.pos);
            vec2_from_json(entry["half_size"], player.half_size);
            player.speed_units_per_sec = entry.value("speed_units_per_sec", PLAYER_MOVE_SPEED_UNITS);
            snapshot.players.push_back(std::move(player));
        }
    }
    return true;
}

CoopStateSnapshot capture_coop_snapshot(const State& state, const std::vector<std::string>& member_ids, std::uint64_t sim_frame) {
    CoopStateSnapshot snapshot;
    snapshot.sim_frame = sim_frame;
    snapshot.bonk = state.bonk;
    snapshot.bonk_serial = state.bonk_serial;
    snapshot.bar_height = state.bar_height;
    snapshot.players.reserve(state.players.size());
    for (std::size_t i = 0; i < state.players.size(); ++i) {
        CoopPlayerSnapshot player;
        if (i < member_ids.size())
            player.member_id = member_ids[i];
        player.pos = state.players[i].pos;
        player.half_size = state.players[i].half_size;
        player.speed_units_per_sec = state.players[i].speed_units_per_sec;
        snapshot.players.push_back(std::move(player));
    }
    return snapshot;
}

void apply_coop_snapshot(const CoopStateSnapshot& snapshot, State& state, std::vector<std::string>& member_ids_out) {
    state.players.clear();
    member_ids_out.clear();
    state.players.reserve(snapshot.players.size());
    member_ids_out.reserve(snapshot.players.size());
    for (const auto& player : snapshot.players) {
        DemoPlayer value;
        value.pos = player.pos;
        value.half_size = player.half_size;
        value.speed_units_per_sec = player.speed_units_per_sec;
        state.players.push_back(value);
        member_ids_out.push_back(player.member_id);
    }
    state.bonk = snapshot.bonk;
    state.bonk_serial = snapshot.bonk_serial;
    state.bar_height = snapshot.bar_height;
}
