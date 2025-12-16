#pragma once

#include "user_profiles.hpp"

struct Player {
    bool has_active_profile = false;
    UserProfile profile;
};

/*
 add a new player to es->players
 returns the index of the new player
*/
int add_player();

/*
 remove a player from es->players by index
*/
void remove_player(int player_index);
