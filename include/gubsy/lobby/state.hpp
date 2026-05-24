#pragma once

#include "gubsy/input/binds_profile.hpp"
#include "gubsy/input/sources.hpp"
#include "gubsy/lobby/session_contract.hpp"

#include <string>
#include <vector>

struct EngineState;
struct UserProfile;
struct InputSettingsProfile;

enum class GubsyLobbyVisibility {
    Private = 0,
    Public = 1,
};

struct GubsyLobbyDeviceAssignment {
    InputSourceType type{InputSourceType::Keyboard};
    int device_id{0};
};

struct GubsyLobbyPlayer {
    int user_profile_id{-1};
    int binds_profile_id{-1};
    int input_settings_profile_id{-1};
    bool ready{false};
    std::vector<GubsyLobbyDeviceAssignment> devices;
};

struct GubsyLobbyState {
    std::string lobby_name{"Local Game"};
    GubsyLobbyVisibility visibility{GubsyLobbyVisibility::Private};
    int max_players{8};
    int selected_player_index{0};
    std::vector<GubsyLobbyPlayer> local_players;

    std::string room_server_url{"http://127.0.0.1:8787"};
    std::string room_code;
    std::string member_id;
    std::string host_secret;
    std::string status_message;
    bool online{false};
    bool is_host{false};
    std::vector<std::string> browse_room_codes;
    SessionContract contract{};
};

GubsyLobbyState& gubsy_lobby_state(EngineState& engine);
const GubsyLobbyState& gubsy_lobby_state(const EngineState& engine);

void gubsy_lobby_ensure_ready(EngineState& engine);
int gubsy_lobby_add_local_player(EngineState& engine);
void gubsy_lobby_remove_local_player(EngineState& engine, int player_index);
void gubsy_lobby_select_player(EngineState& engine, int player_index);

GubsyLobbyPlayer* gubsy_lobby_player(EngineState& engine, int player_index);
const GubsyLobbyPlayer* gubsy_lobby_player(const EngineState& engine, int player_index);
UserProfile* gubsy_lobby_user_profile(EngineState& engine, int player_index);
BindsProfile* gubsy_lobby_binds_profile(EngineState& engine, int player_index);
InputSettingsProfile* gubsy_lobby_input_settings_profile(EngineState& engine, int player_index);

bool gubsy_lobby_set_user_profile(EngineState& engine, int player_index, int profile_id);
bool gubsy_lobby_set_binds_profile(EngineState& engine, int player_index, int profile_id);
bool gubsy_lobby_set_input_settings_profile(EngineState& engine, int player_index, int profile_id);
void gubsy_lobby_toggle_device(EngineState& engine,
                               int player_index,
                               GubsyLobbyDeviceAssignment device);
bool gubsy_lobby_player_has_device(const EngineState& engine,
                                   int player_index,
                                   GubsyLobbyDeviceAssignment device);

bool gubsy_lobby_validate_start(EngineState& engine, std::string& message);
std::string gubsy_lobby_player_label(const EngineState& engine, int player_index);
std::string gubsy_lobby_device_label(GubsyLobbyDeviceAssignment device);
GubsyLobbyDeviceAssignment gubsy_lobby_device_from_input_source(const InputSource& source);
