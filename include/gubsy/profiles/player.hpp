#pragma once

#include "gubsy/input/binds.hpp"
#include "gubsy/lobby/state.hpp"
#include "gubsy/profiles/user_profile.hpp"

#include <vector>

struct EngineState;

struct Player {
    bool has_active_profile = false;
    UserProfile profile;
    int user_profile_id{-1};
    int binds_profile_id{-1};
    int input_settings_profile_id{-1};
    std::vector<GubsyLobbyDeviceAssignment> devices;
};

int add_player(EngineState& engine, int player_index = -1);
void remove_player(EngineState& engine, int player_index);
UserProfile* get_player_user_profile(EngineState& engine, int player_index);
BindsProfile* get_player_binds_profile(EngineState& engine, int player_index);
void set_user_profile_for_player(EngineState& engine, int player_index, int user_profile_id);
