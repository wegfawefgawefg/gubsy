#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "input.hpp"

struct BindsProfile {
    int id;
    std::string name;

    std::vector<std::pair<int, int>> button_binds;      // device_button -> gubsy_action
    std::vector<std::pair<int, int>> analog_1d_binds;   // device_axis -> gubsy_1d_analog
    std::vector<std::pair<int, int>> analog_2d_binds;   // device_stick -> gubsy_2d_analog
};

// Schema entry stores metadata for different bind types
struct ActionSchemaEntry {
    int action_id;
    std::string display_name;
    std::string category;
};

struct Analog1DSchemaEntry {
    int analog_id;
    std::string display_name;
    std::string category;
};

struct Analog2DSchemaEntry {
    int analog_id;
    std::string display_name;
    std::string category;
};

struct BindsSchema {
    std::vector<ActionSchemaEntry> actions;
    std::vector<Analog1DSchemaEntry> analogs_1d;
    std::vector<Analog2DSchemaEntry> analogs_2d;

    void add_action(int action_id, const std::string& display_name, const std::string& category);
    void add_1d_analog(int analog_id, const std::string& display_name, const std::string& category);
    void add_2d_analog(int analog_id, const std::string& display_name, const std::string& category);
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
void bind_button(BindsProfile& profile, GubsyButton device_button, int gubsy_action);

void bind_1d_analog(BindsProfile& profile, int device_axis, int gubsy_1d_analog);
void bind_1d_analog(BindsProfile& profile, Gubsy1DAnalog device_axis, int gubsy_1d_analog);

void bind_2d_analog(BindsProfile& profile, int device_stick, int gubsy_2d_analog);
void bind_2d_analog(BindsProfile& profile, Gubsy2DAnalog device_stick, int gubsy_2d_analog);

/*
 get the global pool of binds profiles
*/
std::vector<BindsProfile>& get_binds_profiles_pool();

/*
 Register the binds schema and reconcile existing binds profiles
 - Reconciles all existing binds profiles with the new schema
 - Removes binds to actions/analogs not in the schema
 - Keeps valid binds that match the schema
*/
void register_binds_schema(const BindsSchema& schema);
