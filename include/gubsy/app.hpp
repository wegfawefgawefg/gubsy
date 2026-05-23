#pragma once

struct GubsyAppConfig {
    bool enable_mods{false};
    bool enable_mod_browser{false};
    bool enable_mod_hot_reload{false};
    bool enable_lua_mod_host{false};
};

inline GubsyAppConfig normalize_gubsy_app_config(GubsyAppConfig config) {
    if (config.enable_mod_browser ||
        config.enable_mod_hot_reload ||
        config.enable_lua_mod_host) {
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
