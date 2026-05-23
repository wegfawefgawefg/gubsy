#pragma once

#include <string>
#include <array>
#include <vector>

#include "src/session_contract.hpp"
#include "src/mods.hpp"

struct EngineState;

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
    int privacy{0};
    int max_players{1};
    int current_players{0};
    SessionContract contract{};
};

struct LobbyOnlineState {
    std::string server_url;
    std::string room_code;
    std::string host_secret;
    std::string member_id;
    std::string status_text;
    std::string last_error;
    bool in_room{false};
    bool is_host{false};
    SessionCompatibility compatibility{SessionCompatibility::Compatible};
    SessionContract contract{};
    std::string last_published_contract_key;
    std::uint64_t synced_content_revision{0};
    std::uint64_t failed_content_revision{0};
    double next_content_retry_at{0.0};
    std::string content_status_text;
    int room_failure_count{0};
    double first_room_failure_at{0.0};
    bool reconnecting{false};
    bool session_closed{false};
    std::string session_close_reason;
    double next_room_poll_at{0.0};
    double next_room_publish_at{0.0};
    double next_rooms_refresh_at{0.0};
    std::vector<LobbyOnlineMember> members;
    std::vector<LobbyDiscoveredRoom> discovered_rooms;
};

struct LobbySession {
    EngineState* engine{nullptr};
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

void lobby_bind_engine(EngineState& engine);
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
void lobby_online_mark_contract_dirty(LobbySession& lobby);
