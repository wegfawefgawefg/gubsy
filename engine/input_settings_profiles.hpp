#pragma once

#include <string>
#include <vector>

struct InputSettingsProfile {
    int id;
    std::string name;

    // Mouse settings
    float mouse_sensitivity;
    bool mouse_invert_x;
    bool mouse_invert_y;

    // Controller settings
    float controller_sensitivity;
    float stick_deadzone;
    float trigger_threshold;
    bool controller_invert_x;
    bool controller_invert_y;
    bool vibration_enabled;
    float vibration_strength;
};

/*
 Load all input settings profiles from disk
*/
std::vector<InputSettingsProfile> load_all_input_settings_profiles();

/*
 Load specific input settings profile by ID
*/
InputSettingsProfile load_input_settings_profile(int profile_id);

/*
 Save input settings profile to disk
 Prevents duplicate names and IDs
*/
bool save_input_settings_profile(const InputSettingsProfile& profile);

/*
 Load all input settings profiles into es->input_settings_profiles pool
*/
bool load_input_settings_profiles_pool();

/*
 Generate random 8-digit ID for input settings profile
*/
int generate_input_settings_profile_id();

/*
 Create and save a default input settings profile with sensible defaults
*/
InputSettingsProfile create_default_input_settings_profile();
