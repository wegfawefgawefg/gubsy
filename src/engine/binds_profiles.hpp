#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "input.hpp"

struct BindsProfiles {
    int id;
    std::string name;

    std::unordered_map<int, GubsyButton> button_binds;
    std::unordered_map<int, GubsyAnalog> axis_binds;
};

std::vector<BindsProfiles> load_all_binds_profiles_metadatas();
BindsProfiles load_binds_profile(int profile_id);
bool save_binds_profile(const BindsProfiles& profile);

void bind_button(int game_button, GubsyButton gubsy_button);
void bind_1d_analog(int game_axis, Gubsy1DAnalog gubsy_axis);
void bind_2d_analog(int game_axis, Gubsy2DAnalog gubsy_axis);

// maybe later
// void bind_analog_threshold_as_button(int game_button, GubsyAnalog gubsy_axis, float threshold, bool above_or_below);
// void bind_analog_delta_over_time_as_button(int game_button, GubsyAnalog gubsy_axis, float delta_threshold, float time_window);
