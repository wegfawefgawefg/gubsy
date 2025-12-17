#pragma once

#include <string>
#include <vector>
struct UserProfile {
    int id;
    std::string name;
    bool guest = false;

    int last_binds_profile_id; // -1 if none
    int last_input_settings_profile_id; // -1 if none
    int last_game_settings_profile_id; // -1 if none
};

/*
 find all user profiles (metadata only, id + name)
 if there arent any, return a blank vector
*/
std::vector<UserProfile> load_all_user_profile_metadatas();

UserProfile load_user_profile(int profile_id);

bool save_user_profile(const UserProfile& profile);

/*
 load all user profiles into the engine state global
*/
bool load_user_profiles_pool();

/*
 generate a random 8-digit user profile id that doesn't conflict with existing ids
*/
int generate_user_profile_id();

/*
 create and save a default user profile
 returns the created profile
*/
UserProfile create_default_user_profile();