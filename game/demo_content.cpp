#include "game/mod_api/demo_content_internal.hpp"

#include "engine/globals.hpp"
#include "game/state.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>
#include <sol/sol.hpp>

namespace {

struct DemoContentEntry {
    std::string mod_id;
    bool has_player{false};
    DemoPlayer player{};
    bool has_bonk{false};
    BonkTarget bonk{};
};

std::vector<DemoContentEntry> g_entries;
DemoPlayer g_default_player{};
BonkTarget g_default_bonk{};
bool g_defaults_captured = false;

void ensure_defaults_captured() {
    if (g_defaults_captured || !ss)
        return;
    if (ss->players.empty())
        ss->players.push_back(DemoPlayer{});
    g_default_player = ss->players[0];
    g_default_bonk = ss->bonk;
    g_defaults_captured = true;
}

glm::vec2 read_vec2(const sol::object& obj, const glm::vec2& fallback) {
    if (!obj.is<sol::table>())
        return fallback;
    sol::table tbl = obj.as<sol::table>();
    glm::vec2 result = fallback;
    result.x = tbl.get_or("x", fallback.x);
    result.y = tbl.get_or("y", fallback.y);
    return result;
}

DemoPlayer read_player(const sol::table& tbl, const DemoPlayer& base) {
    DemoPlayer player = base;
    player.speed_units_per_sec = tbl.get_or("speed", base.speed_units_per_sec);
    if (auto start = tbl.get<sol::optional<sol::table>>("start"))
        player.pos = read_vec2(*start, player.pos);
    if (auto size = tbl.get<sol::optional<sol::table>>("size"))
        player.half_size = read_vec2(*size, player.half_size);
    return player;
}

BonkTarget read_bonk(const sol::table& tbl, const BonkTarget& base) {
    BonkTarget bonk = base;
    if (auto pos = tbl.get<sol::optional<sol::table>>("position"))
        bonk.pos = read_vec2(*pos, bonk.pos);
    if (auto size = tbl.get<sol::optional<sol::table>>("size"))
        bonk.half_size = read_vec2(*size, bonk.half_size);
    bonk.sound_key = tbl.get_or("sound", bonk.sound_key);
    return bonk;
}

} // namespace

namespace demo_content_internal {

void register_override(const std::string& mod_id, const sol::table& tbl) {
    ensure_defaults_captured();
    DemoContentEntry entry;
    entry.mod_id = mod_id;
    if (auto player_tbl = tbl.get<sol::optional<sol::table>>("player")) {
        entry.has_player = true;
        entry.player = read_player(*player_tbl, g_default_player);
    }
    if (auto bonk_tbl = tbl.get<sol::optional<sol::table>>("bonk")) {
        entry.has_bonk = true;
        entry.bonk = read_bonk(*bonk_tbl, g_default_bonk);
    }
    g_entries.push_back(std::move(entry));
}

void remove_override(const std::string& mod_id) {
    g_entries.erase(std::remove_if(g_entries.begin(), g_entries.end(),
                                   [&](const DemoContentEntry& entry) {
                                       return entry.mod_id == mod_id;
                                   }),
                    g_entries.end());
}

void apply_overrides() {
    ensure_defaults_captured();
    if (!ss)
        return;

    DemoPlayer player = g_default_player;
    BonkTarget bonk = g_default_bonk;
    for (const auto& entry : g_entries) {
        if (entry.has_player)
            player = entry.player;
        if (entry.has_bonk)
            bonk = entry.bonk;
    }

    if (ss->players.empty())
        ss->players.push_back(player);
    else
        ss->players[0] = player;
    ss->bonk = bonk;

    if (g_entries.empty())
        std::fprintf(stderr, "[demo] no demo content overrides; using defaults\n");
}

} // namespace demo_content_internal
