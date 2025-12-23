#include "engine/settings_catalog.hpp"

#include "engine/globals.hpp"
#include "engine/game_settings.hpp"
#include "engine/top_level_game_settings.hpp"
#include "engine/user_profiles.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

GameSettings& ensure_active_game_settings_for_player(int player_index) {
    static GameSettings dummy_settings{};
    if (!es)
        return dummy_settings;

    auto ensure_profile_has_settings = [](UserProfile& profile) {
        if (!es)
            return;
        if (profile.last_game_settings_profile_id != -1)
            return;
        GameSettings new_settings = create_game_settings_from_schema();
        es->game_settings_pool.push_back(new_settings);
        profile.last_game_settings_profile_id = new_settings.id;
        for (auto& up : es->user_profiles_pool) {
            if (up.id == profile.id) {
                up.last_game_settings_profile_id = new_settings.id;
                save_user_profile(up);
                break;
            }
        }
        for (std::size_t i = 0; i < es->players.size(); ++i) {
            if (es->players[i].has_active_profile && es->players[i].profile.id == profile.id) {
                es->players[i].profile.last_game_settings_profile_id = new_settings.id;
            }
        }
    };

    UserProfile* target_profile = nullptr;
    if (player_index >= 0 && player_index < static_cast<int>(es->players.size())) {
        Player& p = es->players[static_cast<std::size_t>(player_index)];
        if (p.has_active_profile)
            target_profile = &p.profile;
    }

    if (!target_profile) {
        for (auto& player : es->players) {
            if (player.has_active_profile) {
                target_profile = &player.profile;
                break;
            }
        }
    }

    if (!target_profile) {
        if (!es->user_profiles_pool.empty()) {
            target_profile = &es->user_profiles_pool.front();
        } else {
            es->user_profiles_pool.push_back(create_default_user_profile());
            target_profile = &es->user_profiles_pool.back();
        }
    }

    ensure_profile_has_settings(*target_profile);

    int target_settings_id = target_profile ? target_profile->last_game_settings_profile_id : -1;

    auto find_settings = [&](int id) -> GameSettings* {
        auto it = std::find_if(es->game_settings_pool.begin(),
                               es->game_settings_pool.end(),
                               [&](const GameSettings& settings) { return settings.id == id; });
        if (it != es->game_settings_pool.end())
            return &(*it);
        GameSettings loaded = load_game_settings(id);
        if (loaded.id != -1) {
            es->game_settings_pool.push_back(loaded);
            return &es->game_settings_pool.back();
        }
        return nullptr;
    };

    GameSettings* profile_settings = find_settings(target_settings_id);
    if (!profile_settings) {
        GameSettings replacement = create_game_settings_from_schema();
        es->game_settings_pool.push_back(replacement);
        profile_settings = &es->game_settings_pool.back();
        if (target_profile) {
            target_profile->last_game_settings_profile_id = profile_settings->id;
            for (auto& up : es->user_profiles_pool) {
                if (up.id == target_profile->id) {
                    up.last_game_settings_profile_id = profile_settings->id;
                    save_user_profile(up);
                    break;
                }
            }
            for (auto& player : es->players) {
                if (player.has_active_profile && player.profile.id == target_profile->id) {
                    player.profile.last_game_settings_profile_id = profile_settings->id;
                }
            }
        }
    }

    return profile_settings ? *profile_settings : dummy_settings;
}

SettingsValue* resolve_value(const SettingMetadata& meta, GameSettings& profile_settings) {
    if (!es)
        return nullptr;
    if (meta.scope == SettingScope::Install) {
        auto& map = es->top_level_game_settings.settings;
        auto [it, inserted] = map.emplace(meta.key, meta.default_value);
        if (inserted)
            save_top_level_game_settings(es->top_level_game_settings);
        return &it->second;
    }
    auto& settings_map = profile_settings.settings;
    auto [it, inserted] = settings_map.emplace(meta.key, meta.default_value);
    if (inserted)
        save_game_settings(profile_settings);
    return &it->second;
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

SettingsCatalog build_settings_catalog(int player_index) {
    SettingsCatalog catalog;
    if (!es)
        return catalog;

    GameSettings& active_profile_settings = ensure_active_game_settings_for_player(player_index);
    catalog.profile_settings = &active_profile_settings;
    const SettingsSchema& schema = get_settings_schema();

    for (const auto& meta : schema.entries()) {
        SettingsValue* value_ptr = resolve_value(meta, active_profile_settings);
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
