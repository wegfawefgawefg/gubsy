#pragma once

#include "user_profiles.hpp"

struct BindsProfile; // Forward declaration
struct EngineState;

struct Player {
    bool has_active_profile = false;
    UserProfile profile;
};

/*
 add a new player to engine.players
 returns the index of the new player
*/
int add_player(EngineState& engine, int player_index = -1);

/*
 remove a player from engine.players by index
*/
void remove_player(EngineState& engine, int player_index);

/*
 get the user profile for a given player
*/
UserProfile* get_player_user_profile(EngineState& engine, int player_index);

/*
 get the binds profile for a given player
*/
BindsProfile* get_player_binds_profile(EngineState& engine, int player_index);


/*
 set the user profile for a given player
*/
void set_user_profile_for_player(EngineState& engine, int player_index, int user_profile_id);
