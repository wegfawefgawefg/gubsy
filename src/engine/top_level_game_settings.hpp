#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>


// Top-level settings value can be int, float, or string
using TopLevelSettingsValue = std::variant<int, float, std::string>;

struct TopLevelGameSettings {
    std::unordered_map<std::string, TopLevelSettingsValue> settings;
};

// Schema entry stores the key name and default value
struct TopLevelSchemaEntry {
    std::string key;
    TopLevelSettingsValue default_value;
};

struct TopLevelGameSettingsSchema {
    std::vector<TopLevelSchemaEntry> entries;

    void add_int(const std::string& key, int default_value);
    void add_float(const std::string& key, float default_value);
    void add_string(const std::string& key, const std::string& default_value);
};

/*
 Load top-level game settings from disk
 Returns empty settings if file doesn't exist
*/
TopLevelGameSettings load_top_level_game_settings();

/*
 Save top-level game settings to disk
*/
bool save_top_level_game_settings(const TopLevelGameSettings& settings);

/*
 Load top-level settings into es->top_level_game_settings
*/
bool load_top_level_game_settings_into_state();

// Helper functions for setting values
void set_top_level_setting_int(TopLevelGameSettings& settings, const std::string& key, int value);
void set_top_level_setting_float(TopLevelGameSettings& settings, const std::string& key, float value);
void set_top_level_setting_string(TopLevelGameSettings& settings, const std::string& key, const std::string& value);

// Helper functions for getting values (with defaults)
int get_top_level_setting_int(const TopLevelGameSettings& settings, const std::string& key, int default_value = 0);
float get_top_level_setting_float(const TopLevelGameSettings& settings, const std::string& key, float default_value = 0.0f);
std::string get_top_level_setting_string(const TopLevelGameSettings& settings, const std::string& key, const std::string& default_value = "");

/*
 Register the global settings schema and reconcile existing settings
 - If no settings file exists, creates one with default values
 - If settings file exists, reconciles it with schema (keeps compatible values, removes incompatible ones, adds missing ones)
*/
void register_global_settings_schema(const TopLevelGameSettingsSchema& schema);
