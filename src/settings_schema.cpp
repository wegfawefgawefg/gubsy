#include "src/settings_schema.hpp"

#include "src/engine_state.hpp"
#include "src/game_settings.hpp"
#include "src/top_level_game_settings.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

SettingsSchema g_settings_schema;

bool reconcile_top_level_settings(EngineState& engine, const SettingsSchema& schema) {
    bool has_install_entries = false;
    TopLevelGameSettings top = load_top_level_game_settings();
    bool changed = false;

    for (const auto& entry : schema.entries()) {
        if (entry.scope != SettingScope::Install)
            continue;
        has_install_entries = true;

        auto it = top.settings.find(entry.key);
        if (it == top.settings.end() || it->second.index() != entry.default_value.index()) {
            top.settings[entry.key] = entry.default_value;
            changed = true;
        }
    }

    if (!has_install_entries) {
        // Still make sure the engine state sees whatever is on disk.
        load_top_level_game_settings_into_state(engine);
        return false;
    }

    if (changed)
        save_top_level_game_settings(top);

    load_top_level_game_settings_into_state(engine);
    return changed;
}

void reconcile_profile_settings(EngineState& engine, const SettingsSchema& schema) {
    bool has_profile_entries = false;
    for (const auto& entry : schema.entries()) {
        if (entry.scope == SettingScope::Profile) {
            has_profile_entries = true;
            break;
        }
    }

    if (!has_profile_entries) {
        load_game_settings_pool(engine);
        return;
    }

    auto all_settings = load_all_game_settings();
    for (auto& settings : all_settings) {
        bool changed = false;

        for (const auto& entry : schema.entries()) {
            if (entry.scope != SettingScope::Profile)
                continue;
            auto it = settings.settings.find(entry.key);
            if (it == settings.settings.end() || it->second.index() != entry.default_value.index()) {
                settings.settings[entry.key] = entry.default_value;
                changed = true;
            }
        }

        if (changed)
            save_game_settings(settings);
    }

    load_game_settings_pool(engine);
}

} // namespace

SettingMetadata& SettingsSchema::add_setting(SettingMetadata meta) {
    entries_.push_back(std::move(meta));
    return entries_.back();
}

const SettingsSchema& get_settings_schema() {
    return g_settings_schema;
}

void register_settings_schema(EngineState& engine, const SettingsSchema& schema) {
    auto& dest = g_settings_schema.entries();
    for (const auto& entry : schema.entries()) {
        auto it = std::find_if(dest.begin(), dest.end(),
                               [&](const SettingMetadata& existing) { return existing.key == entry.key; });
        if (it != dest.end()) {
            *it = entry;
        } else {
            g_settings_schema.add_setting(entry);
        }
    }
    reconcile_top_level_settings(engine, g_settings_schema);
    reconcile_profile_settings(engine, g_settings_schema);
}
