#pragma once

#include <string>
#include <vector>

struct EngineState;

struct UserProfile {
    int id;
    std::string name;
    bool guest = false;

    int last_binds_profile_id;
    int last_input_settings_profile_id;
    int last_game_settings_profile_id;
};

std::vector<UserProfile> load_all_user_profile_metadatas();
UserProfile load_user_profile(int profile_id);
bool save_user_profile(const UserProfile& profile);
bool delete_user_profile(int profile_id);
bool load_user_profiles_pool(EngineState& engine);
int generate_user_profile_id();
UserProfile create_default_user_profile();
std::vector<UserProfile>& get_user_profiles_pool(EngineState& engine);
