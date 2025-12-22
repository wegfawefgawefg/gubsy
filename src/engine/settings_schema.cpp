#include "engine/settings_schema.hpp"

#include "engine/game_settings.hpp"
#include "engine/globals.hpp"
#include "engine/top_level_game_settings.hpp"

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

SettingsSchema g_settings_schema;

bool reconcile_top_level_settings(const SettingsSchema& schema) {
    bool has_install_entries = false;
    TopLevelGameSettings top = load_top_level_game_settings();
    std::unordered_map<std::string, SettingsValue> reconciled;
    std::unordered_set<std::string> allowed_keys;
    bool changed = false;

    for (const auto& entry : schema.entries()) {
        if (entry.scope != SettingScope::Install)
            continue;
        has_install_entries = true;
        allowed_keys.insert(entry.key);

        auto it = top.settings.find(entry.key);
        if (it != top.settings.end() && it->second.index() == entry.default_value.index()) {
            reconciled[entry.key] = it->second;
        } else {
            reconciled[entry.key] = entry.default_value;
            changed = true;
        }
    }

    if (!has_install_entries) {
        // Still make sure the engine state sees whatever is on disk.
        load_top_level_game_settings_into_state();
        return false;
    }

    for (const auto& [key, value] : top.settings) {
        if (!allowed_keys.count(key)) {
            changed = true;
        }
    }

    if (changed) {
        top.settings = std::move(reconciled);
        save_top_level_game_settings(top);
    }

    load_top_level_game_settings_into_state();
    return changed;
}

void reconcile_profile_settings(const SettingsSchema& schema) {
    bool has_profile_entries = false;
    for (const auto& entry : schema.entries()) {
        if (entry.scope == SettingScope::Profile) {
            has_profile_entries = true;
            break;
        }
    }

    if (!has_profile_entries) {
        load_game_settings_pool();
        return;
    }

    auto all_settings = load_all_game_settings();
    for (auto& settings : all_settings) {
        std::unordered_map<std::string, SettingsValue> reconciled;
        bool changed = false;

        for (const auto& entry : schema.entries()) {
            if (entry.scope != SettingScope::Profile)
                continue;
            auto it = settings.settings.find(entry.key);
            if (it != settings.settings.end() && it->second.index() == entry.default_value.index()) {
                reconciled[entry.key] = it->second;
            } else {
                reconciled[entry.key] = entry.default_value;
                changed = true;
            }
        }

        if (reconciled.size() != settings.settings.size())
            changed = true;

        if (changed) {
            settings.settings = std::move(reconciled);
            save_game_settings(settings);
        }
    }

    load_game_settings_pool();
}

} // namespace

SettingMetadata& SettingsSchema::add_setting(SettingMetadata meta) {
    entries_.push_back(std::move(meta));
    return entries_.back();
}

const SettingsSchema& get_settings_schema() {
    return g_settings_schema;
}

void register_settings_schema(const SettingsSchema& schema) {
    g_settings_schema = schema;
    reconcile_top_level_settings(g_settings_schema);
    reconcile_profile_settings(g_settings_schema);
}
