#pragma once

#include "engine/binds_ui_helpers.hpp"
#include "engine/input.hpp"

#include <ginput/ginput.hpp>
#include <string>
#include <vector>

struct EngineState;

using BindsProfile = ginput::InputProfile;
using BindsSchema = ginput::Schema;

std::vector<BindsProfile> load_all_binds_profiles();
BindsProfile load_binds_profile(int profile_id);
bool save_binds_profile(const BindsProfile& profile);
bool load_binds_profiles_pool(EngineState& engine);
int generate_binds_profile_id();
BindsProfile create_default_binds_profile();

void bind_button(BindsProfile& profile, int device_button, int gubsy_action);
void bind_button(BindsProfile& profile, GubsyButton device_button, int gubsy_action);

void bind_1d_analog(BindsProfile& profile, int device_axis, int gubsy_1d_analog);
void bind_1d_analog(BindsProfile& profile, Gubsy1DAnalog device_axis, int gubsy_1d_analog);

void bind_2d_analog(BindsProfile& profile, int device_stick, int gubsy_2d_analog);
void bind_2d_analog(BindsProfile& profile, Gubsy2DAnalog device_stick, int gubsy_2d_analog);

bool remove_bind_at(BindsProfile& profile, BindsActionType type, int index);
bool replace_bind_at(BindsProfile& profile, BindsActionType type, int index, int device_code,
                     int action_id);
void remove_binds_for_action(BindsProfile& profile, BindsActionType type, int action_id);
void clear_binds(BindsProfile& profile);

std::vector<BindsProfile>& get_binds_profiles_pool(EngineState& engine);

void register_binds_schema(EngineState& engine, const BindsSchema& schema);
const BindsSchema& get_binds_schema();
