#pragma once

#include "gubsy/settings/schema.hpp"
#include "gubsy/settings/types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct EngineState;
struct GameSettings;
struct UserProfile;

struct SettingsCatalogEntry {
    const SettingMetadata* metadata = nullptr;
    SettingsValue* value = nullptr;
    bool install_scope = false;
};

struct SettingsCatalog {
    std::unordered_map<std::string, std::vector<SettingsCatalogEntry>> categories;
    std::vector<SettingsCatalogEntry> install_entries;
    std::vector<SettingsCatalogEntry> profile_entries;
    GameSettings* profile_settings{nullptr};
    UserProfile* user_profile{nullptr};
};

SettingsCatalog build_settings_catalog(EngineState& engine, int player_index = 0);
