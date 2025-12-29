#pragma once

#include <string>
#include <array>
#include <vector>

#include "engine/mods.hpp"

struct LobbySession {
    std::string session_name;
    int privacy{0}; // 0 = Solo, 1 = Couch, 2 = Friends, 3 = Invite Only, 4 = Anyone
    int scenario_index{0};
    int max_players{4};
    std::string seed;
    bool seed_randomized{true};
    bool name_initialized{false};
    std::array<bool, 4> local_players{{true, false, false, false}};
    int selected_player_index{0};
    std::vector<LobbyModEntry> mods;
};

LobbySession& lobby_state();
const LobbySession& lobby_state_const();

void lobby_reset_defaults();
void lobby_refresh_mods();
std::vector<std::string> lobby_enabled_mod_ids();
int lobby_local_player_count();
