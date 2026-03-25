#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "engine/settings_types.hpp"

struct GameSettings {
    int id;
    std::string name;
    std::unordered_map<std::string, SettingsValue> settings;
};

/*
 Load all game settings from disk
*/
std::vector<GameSettings> load_all_game_settings();

/*
 Load specific game settings by ID
*/
GameSettings load_game_settings(int settings_id);

/*
 Save game settings to disk
 Prevents duplicate names and IDs
*/
bool save_game_settings(const GameSettings& settings);

/*
 Load all game settings into es->game_settings_pool
*/
bool load_game_settings_pool();

/*
 Generate random 8-digit ID for game settings
*/
int generate_game_settings_id();

/*
 Create and save default (empty) game settings
*/
GameSettings create_default_game_settings();

// Helper functions for setting values
void set_game_setting_int(GameSettings& settings, const std::string& key, int value);
void set_game_setting_float(GameSettings& settings, const std::string& key, float value);
void set_game_setting_string(GameSettings& settings, const std::string& key, const std::string& value);
void set_game_setting_vec2(GameSettings& settings, const std::string& key, float x, float y);

// Helper functions for getting values (with defaults)
int get_game_setting_int(const GameSettings& settings, const std::string& key, int default_value = 0);
float get_game_setting_float(const GameSettings& settings, const std::string& key, float default_value = 0.0f);
std::string get_game_setting_string(const GameSettings& settings, const std::string& key, const std::string& default_value = "");
SettingsVec2 get_game_setting_vec2(const GameSettings& settings, const std::string& key, float default_x = 0.0f, float default_y = 0.0f);

/*
 Create a new game settings profile from the registered schema
 Returns a profile with all default values
*/
GameSettings create_game_settings_from_schema();
