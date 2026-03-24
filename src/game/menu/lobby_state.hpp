#pragma once

#include <string>
#include <array>
#include <vector>

#include "engine/mods.hpp"

struct PlayerDeviceKey {
    int type{0};
    int id{0};
};

struct LobbyOnlineMember {
    std::string member_id;
    std::string display_name;
    bool is_host{false};
    bool is_local{false};
};

struct LobbyDiscoveredRoom {
    std::string room_code;
    std::string session_name;
    std::string host_name;
    std::string session_phase{"lobby"};
    std::string realtime_endpoint;
    std::string game_version;
    std::string mod_hash;
    int privacy{0};
    int max_players{1};
    int current_players{0};
    bool in_game{false};
};

struct LobbyOnlineState {
    std::string server_url;
    std::string room_code;
    std::string host_secret;
    std::string member_id;
    std::string status_text;
    std::string last_error;
    std::string session_phase{"lobby"};
    std::string realtime_endpoint;
    bool in_room{false};
    bool is_host{false};
    bool in_game{false};
    double next_room_poll_at{0.0};
    double next_room_publish_at{0.0};
    double next_rooms_refresh_at{0.0};
    std::vector<LobbyOnlineMember> members;
    std::vector<LobbyDiscoveredRoom> discovered_rooms;
};

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
    std::vector<std::vector<PlayerDeviceKey>> player_devices;
    std::vector<int> cached_profile_ids;
    std::vector<LobbyModEntry> mods;
    LobbyOnlineState online;
};

LobbySession& lobby_state();
const LobbySession& lobby_state_const();

void lobby_reset_defaults();
void lobby_refresh_mods();
std::vector<std::string> lobby_enabled_mod_ids();
std::string lobby_enabled_mod_signature();
std::string lobby_local_player_name();
int lobby_local_player_count();
void lobby_ensure_player_devices(int player_index);
bool lobby_device_enabled(int player_index, int type, int id);
void lobby_toggle_device(int player_index, int type, int id);
const char* lobby_session_phase(const LobbySession& lobby);
bool lobby_online_ready_to_enter_game(const LobbySession& lobby);
