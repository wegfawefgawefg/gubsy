#pragma once

#include "gubsy/settings/types.hpp"

#include <string>
#include <unordered_map>

struct EngineState;

struct TopLevelGameSettings {
    std::unordered_map<std::string, SettingsValue> settings;
};

TopLevelGameSettings load_top_level_game_settings();
bool save_top_level_game_settings(const TopLevelGameSettings& settings);
bool load_top_level_game_settings_into_state(EngineState& engine);

void set_top_level_setting_int(TopLevelGameSettings& settings, const std::string& key, int value);
void set_top_level_setting_float(TopLevelGameSettings& settings, const std::string& key, float value);
void set_top_level_setting_string(TopLevelGameSettings& settings,
                                  const std::string& key,
                                  const std::string& value);

int get_top_level_setting_int(const TopLevelGameSettings& settings,
                              const std::string& key,
                              int default_value = 0);
float get_top_level_setting_float(const TopLevelGameSettings& settings,
                                  const std::string& key,
                                  float default_value = 0.0f);
std::string get_top_level_setting_string(const TopLevelGameSettings& settings,
                                         const std::string& key,
                                         const std::string& default_value = "");
