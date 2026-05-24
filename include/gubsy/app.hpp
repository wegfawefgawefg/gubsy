#pragma once

#include "gubsy/menu/types.hpp"

#include <cstdint>
#include <string>

struct GubsyAppConfig {
    bool enable_mods{false};
    bool enable_mod_browser{false};
    bool enable_mod_hot_reload{false};
    bool enable_lua_mod_host{false};

    // Empty paths keep Gubsy's default repo-local roots. Hosts can set only the
    // roots they own, such as data_root for profile/settings/input persistence.
    std::string project_root;
    std::string data_root;
    std::string game_root;
    std::string tools_root;
    std::string engine_assets_root;

    std::string window_title{"Gubsy"};
    std::int32_t window_width{1280};
    std::int32_t window_height{720};
    std::int32_t render_width{1280};
    std::int32_t render_height{720};
    bool utility_window{false};
    bool always_on_top{false};
    bool resizable_window{true};
};

inline GubsyAppConfig normalize_gubsy_app_config(GubsyAppConfig config) {
    if (config.enable_mod_browser || config.enable_mod_hot_reload || config.enable_lua_mod_host) {
        config.enable_mods = true;
    }
    if (!config.enable_mods) {
        config.enable_mod_browser = false;
        config.enable_mod_hot_reload = false;
        config.enable_lua_mod_host = false;
    }
    return config;
}

struct GubsyAppHooks {
    void* app_context{nullptr};
    GubsyAppConfig config{};
    void (*on_mods_changed)(void* app_context){nullptr};
};

struct GubsyMainMenuCommands {
    MenuCommandId start_game{kMenuIdInvalid};
    MenuCommandId quit{kMenuIdInvalid};
};
