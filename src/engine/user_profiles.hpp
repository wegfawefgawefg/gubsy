#pragma once

#include <string>
#include <vector>
struct UserProfile {
    int id;
    std::string name;
    int last_settings_profile;
    int last_binds_profile;
};

std::vector<UserProfile> load_all_user_profile_metadatas();
UserProfile load_user_profile(int profile_id);
bool save_user_profile(const UserProfile& profile);