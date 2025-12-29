#include "game/menu/lobby_state.hpp"

#include <algorithm>

#include "engine/globals.hpp"

namespace {

constexpr int kMinLobbyPlayers = 1;
constexpr int kMaxLobbyPlayers = 64;

LobbySession g_lobby;

bool is_required_mod(const ModInfo& info) {
    return info.required || info.name == "base";
}

} // namespace

LobbySession& lobby_state() {
    return g_lobby;
}

const LobbySession& lobby_state_const() {
    return g_lobby;
}

void lobby_reset_defaults() {
    g_lobby = LobbySession{};
    g_lobby.max_players = std::clamp(g_lobby.max_players, kMinLobbyPlayers, kMaxLobbyPlayers);
}

void lobby_refresh_mods() {
    std::vector<LobbyModEntry> fresh;
    if (mm) {
        fresh.reserve(mm->mods.size());
        for (const auto& mod : mm->mods) {
            LobbyModEntry entry;
            entry.id = mod.name;
            entry.title = mod.title.empty() ? mod.name : mod.title;
            entry.author = mod.author;
            entry.description = mod.description;
            entry.dependencies = mod.deps;
            entry.required = is_required_mod(mod);
            entry.enabled = mod.enabled || entry.required;
            fresh.push_back(std::move(entry));
        }
    }

    for (auto& entry : fresh) {
        auto it = std::find_if(g_lobby.mods.begin(), g_lobby.mods.end(),
                               [&](const LobbyModEntry& prev) { return prev.id == entry.id; });
        if (it != g_lobby.mods.end()) {
            entry.enabled = it->enabled || entry.required;
        }
    }

    g_lobby.mods = std::move(fresh);
    g_lobby.max_players = std::clamp(g_lobby.max_players, kMinLobbyPlayers, kMaxLobbyPlayers);
}

std::vector<std::string> lobby_enabled_mod_ids() {
    std::vector<std::string> ids;
    for (const auto& entry : g_lobby.mods) {
        if (entry.enabled || entry.required)
            ids.push_back(entry.id);
    }
    return ids;
}

int lobby_local_player_count() {
    if (es)
        return std::max(1, static_cast<int>(es->players.size()));
    int count = 0;
    for (bool enabled : g_lobby.local_players) {
        if (enabled)
            ++count;
    }
    return std::max(1, count);
}
