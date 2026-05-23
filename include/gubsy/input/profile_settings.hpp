#pragma once

#include <string>
#include <vector>

struct EngineState;

struct InputSettingsProfile {
    int id;
    std::string name;

    float mouse_sensitivity;
    bool mouse_invert_x;
    bool mouse_invert_y;

    float controller_sensitivity;
    float stick_deadzone;
    float trigger_threshold;
    bool controller_invert_x;
    bool controller_invert_y;
    bool vibration_enabled;
    float vibration_strength;
};

std::vector<InputSettingsProfile> load_all_input_settings_profiles();
InputSettingsProfile load_input_settings_profile(int profile_id);
bool save_input_settings_profile(const InputSettingsProfile& profile);
bool load_input_settings_profiles_pool(EngineState& engine);
int generate_input_settings_profile_id();
InputSettingsProfile create_default_input_settings_profile();
