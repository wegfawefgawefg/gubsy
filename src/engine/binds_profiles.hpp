#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "input.hpp"

struct BindsProfile {
    int id;
    std::string name;

    std::unordered_map<int, int> button_binds;      // device_button -> gubsy_action
    std::unordered_map<int, int> analog_1d_binds;   // device_axis -> gubsy_1d_analog
    std::unordered_map<int, int> analog_2d_binds;   // device_stick -> gubsy_2d_analog
};

/*
 Load all binds profiles from disk
*/
std::vector<BindsProfile> load_all_binds_profiles();

/*
 Load specific binds profile by ID
*/
BindsProfile load_binds_profile(int profile_id);

/*
 Save binds profile to disk
 Prevents duplicate names and IDs
*/
bool save_binds_profile(const BindsProfile& profile);

/*
 Load all binds profiles into es->binds_profiles pool
*/
bool load_binds_profiles_pool();

/*
 Generate random 8-digit ID for binds profile
*/
int generate_binds_profile_id();

/*
 Create and save a default (empty) binds profile
*/
BindsProfile create_default_binds_profile();

// Helper functions for setting binds
void bind_button(BindsProfile& profile, int device_button, int gubsy_action);
void bind_1d_analog(BindsProfile& profile, int device_axis, int gubsy_1d_analog);
void bind_2d_analog(BindsProfile& profile, int device_stick, int gubsy_2d_analog);
