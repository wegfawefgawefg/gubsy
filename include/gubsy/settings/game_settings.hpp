#pragma once

#include "gubsy/settings/types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct EngineState;

struct GameSettings {
    int id;
    std::string name;
    std::unordered_map<std::string, SettingsValue> settings;
};

std::vector<GameSettings> load_all_game_settings();
GameSettings load_game_settings(int settings_id);
bool save_game_settings(const GameSettings& settings);
bool load_game_settings_pool(EngineState& engine);
int generate_game_settings_id();
GameSettings create_default_game_settings();

void set_game_setting_int(GameSettings& settings, const std::string& key, int value);
void set_game_setting_float(GameSettings& settings, const std::string& key, float value);
void set_game_setting_string(GameSettings& settings, const std::string& key, const std::string& value);
void set_game_setting_vec2(GameSettings& settings, const std::string& key, float x, float y);

int get_game_setting_int(const GameSettings& settings, const std::string& key, int default_value = 0);
float get_game_setting_float(const GameSettings& settings, const std::string& key, float default_value = 0.0f);
std::string get_game_setting_string(const GameSettings& settings,
                                    const std::string& key,
                                    const std::string& default_value = "");
SettingsVec2 get_game_setting_vec2(const GameSettings& settings,
                                    const std::string& key,
                                    float default_x = 0.0f,
                                    float default_y = 0.0f);

GameSettings create_game_settings_from_schema();
