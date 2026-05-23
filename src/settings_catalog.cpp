#include "src/settings_catalog.hpp"

#include "src/engine_state.hpp"
#include "src/game_settings.hpp"
#include "src/top_level_game_settings.hpp"
#include "src/user_profiles.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

GameSettings& ensure_active_game_settings_for_player(EngineState& engine,
                                                     int player_index,
                                                     UserProfile** out_profile = nullptr) {
    static GameSettings dummy_settings{};
    auto ensure_profile_has_settings = [&](UserProfile& profile) {
        if (profile.last_game_settings_profile_id != -1)
            return;
        GameSettings new_settings = create_game_settings_from_schema();
        engine.game_settings_pool.push_back(new_settings);
        profile.last_game_settings_profile_id = new_settings.id;
        for (auto& up : engine.user_profiles_pool) {
            if (up.id == profile.id) {
                up.last_game_settings_profile_id = new_settings.id;
                save_user_profile(up);
                break;
            }
        }
        for (std::size_t i = 0; i < engine.players.size(); ++i) {
            if (engine.players[i].has_active_profile && engine.players[i].profile.id == profile.id) {
                engine.players[i].profile.last_game_settings_profile_id = new_settings.id;
            }
        }
    };

    UserProfile* target_profile = nullptr;
    if (player_index >= 0 && player_index < static_cast<int>(engine.players.size())) {
        Player& p = engine.players[static_cast<std::size_t>(player_index)];
        if (p.has_active_profile)
            target_profile = &p.profile;
    }

    if (!target_profile) {
        for (auto& player : engine.players) {
            if (player.has_active_profile) {
                target_profile = &player.profile;
                break;
            }
        }
    }

    if (!target_profile) {
        if (!engine.user_profiles_pool.empty()) {
            target_profile = &engine.user_profiles_pool.front();
        } else {
            engine.user_profiles_pool.push_back(create_default_user_profile());
            target_profile = &engine.user_profiles_pool.back();
        }
    }

    if (out_profile)
        *out_profile = target_profile;

    ensure_profile_has_settings(*target_profile);

    int target_settings_id = target_profile ? target_profile->last_game_settings_profile_id : -1;

    auto find_settings = [&](int id) -> GameSettings* {
        auto it = std::find_if(engine.game_settings_pool.begin(),
                               engine.game_settings_pool.end(),
                               [&](const GameSettings& settings) { return settings.id == id; });
        if (it != engine.game_settings_pool.end())
            return &(*it);
        GameSettings loaded = load_game_settings(id);
        if (loaded.id != -1) {
            engine.game_settings_pool.push_back(loaded);
            return &engine.game_settings_pool.back();
        }
        return nullptr;
    };

    GameSettings* profile_settings = find_settings(target_settings_id);
    if (!profile_settings) {
        GameSettings replacement = create_game_settings_from_schema();
        engine.game_settings_pool.push_back(replacement);
        profile_settings = &engine.game_settings_pool.back();
        if (target_profile) {
            target_profile->last_game_settings_profile_id = profile_settings->id;
            for (auto& up : engine.user_profiles_pool) {
                if (up.id == target_profile->id) {
                    up.last_game_settings_profile_id = profile_settings->id;
                    save_user_profile(up);
                    break;
                }
            }
            for (auto& player : engine.players) {
                if (player.has_active_profile && player.profile.id == target_profile->id) {
                    player.profile.last_game_settings_profile_id = profile_settings->id;
                }
            }
        }
    }

    if (out_profile && !target_profile && profile_settings) {
        for (auto& up : engine.user_profiles_pool) {
            if (up.last_game_settings_profile_id == profile_settings->id) {
                *out_profile = &up;
                break;
            }
        }
    }

    return profile_settings ? *profile_settings : dummy_settings;
}

SettingsValue* resolve_value(const SettingMetadata& meta, GameSettings& profile_settings) {
    if (meta.scope == SettingScope::Install) {
        return nullptr;
    }
    auto& settings_map = profile_settings.settings;
    auto [it, inserted] = settings_map.emplace(meta.key, meta.default_value);
    if (inserted)
        save_game_settings(profile_settings);
    return &it->second;
}

SettingsValue* resolve_value(EngineState& engine,
                             const SettingMetadata& meta,
                             GameSettings& profile_settings) {
    if (meta.scope == SettingScope::Install) {
        auto& map = engine.top_level_game_settings.settings;
        auto [it, inserted] = map.emplace(meta.key, meta.default_value);
        if (inserted)
            save_top_level_game_settings(engine.top_level_game_settings);
        return &it->second;
    }
    return resolve_value(meta, profile_settings);
}

void coerce_value_type(const SettingMetadata& meta, SettingsValue& value) {
    if (meta.widget.kind == SettingWidgetKind::Slider) {
        if (std::string* sv = std::get_if<std::string>(&value)) {
            char* end_ptr = nullptr;
            float parsed = std::strtof(sv->c_str(), &end_ptr);
            if (end_ptr != sv->c_str()) {
                value = parsed;
            } else {
                std::string lower = *sv;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (lower == "unlimited")
                    value = 0.0f;
            }
        } else if (int* iv = std::get_if<int>(&value)) {
            value = static_cast<float>(*iv);
        }
    }
}

} // namespace

SettingsCatalog build_settings_catalog(EngineState& engine, int player_index) {
    SettingsCatalog catalog;
    UserProfile* active_profile = nullptr;
    GameSettings& active_profile_settings =
        ensure_active_game_settings_for_player(engine, player_index, &active_profile);
    catalog.profile_settings = &active_profile_settings;
    catalog.user_profile = active_profile;
    const SettingsSchema& schema = get_settings_schema();

    for (const auto& meta : schema.entries()) {
        SettingsValue* value_ptr = resolve_value(engine, meta, active_profile_settings);
        if (!value_ptr)
            continue;
        coerce_value_type(meta, *value_ptr);

        SettingsCatalogEntry entry{&meta, value_ptr, meta.scope == SettingScope::Install};

        if (entry.install_scope)
            catalog.install_entries.push_back(entry);
        else
            catalog.profile_entries.push_back(entry);

        if (meta.categories.empty()) {
            catalog.categories["General"].push_back(entry);
        } else {
            for (const auto& category : meta.categories) {
                catalog.categories[category].push_back(entry);
            }
        }
    }

    return catalog;
}
