#include "game/menu/lobby_state.hpp"

#include <algorithm>

#include "engine/globals.hpp"
#include "engine/input_sources.hpp"

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

void lobby_ensure_player_devices(int player_index) {
    if (player_index < 0)
        return;
    if (static_cast<std::size_t>(player_index) >= g_lobby.player_devices.size())
        g_lobby.player_devices.resize(static_cast<std::size_t>(player_index) + 1);
    auto& devices = g_lobby.player_devices[static_cast<std::size_t>(player_index)];
    if (!devices.empty())
        return;
    if (!es)
        return;
    for (const auto& src : es->input_sources) {
        devices.push_back(PlayerDeviceKey{static_cast<int>(src.type), src.device_id.id});
    }
}

bool lobby_device_enabled(int player_index, int type, int id) {
    if (player_index < 0)
        return false;
    if (static_cast<std::size_t>(player_index) >= g_lobby.player_devices.size())
        return false;
    const auto& devices = g_lobby.player_devices[static_cast<std::size_t>(player_index)];
    for (const auto& key : devices) {
        if (key.type == type && key.id == id)
            return true;
    }
    return false;
}

void lobby_toggle_device(int player_index, int type, int id) {
    if (player_index < 0)
        return;
    if (static_cast<std::size_t>(player_index) >= g_lobby.player_devices.size())
        g_lobby.player_devices.resize(static_cast<std::size_t>(player_index) + 1);
    auto& devices = g_lobby.player_devices[static_cast<std::size_t>(player_index)];
    for (auto it = devices.begin(); it != devices.end(); ++it) {
        if (it->type == type && it->id == id) {
            devices.erase(it);
            return;
        }
    }
    devices.push_back(PlayerDeviceKey{type, id});
}
