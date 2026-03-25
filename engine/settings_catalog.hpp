#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "engine/settings_schema.hpp"
#include "engine/settings_types.hpp"

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

SettingsCatalog build_settings_catalog(int player_index = 0);
