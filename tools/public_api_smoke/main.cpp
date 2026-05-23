#include <gubsy/app.hpp>
#include <gubsy/engine_state.hpp>
#include <gubsy/input/binds.hpp>
#include <gubsy/layout/layout.hpp>
#include <gubsy/lobby/session.hpp>
#include <gubsy/menu/menu.hpp>
#include <gubsy/profiles/profiles.hpp>
#include <gubsy/run.hpp>
#include <gubsy/settings/settings.hpp>

int main() {
    GubsyAppHooks hooks{};
    if (hooks.config.enable_mods ||
        hooks.config.enable_mod_browser ||
        hooks.config.enable_mod_hot_reload ||
        hooks.config.enable_lua_mod_host) {
        return 1;
    }

    GubsyAppConfig browser_only{};
    browser_only.enable_mod_browser = true;
    GubsyAppConfig normalized = normalize_gubsy_app_config(browser_only);
    if (!normalized.enable_mods || !normalized.enable_mod_browser) {
        return 2;
    }

    GubsyAppConfig no_mods{};
    normalized = normalize_gubsy_app_config(no_mods);
    if (normalized.enable_mods ||
        normalized.enable_mod_browser ||
        normalized.enable_mod_hot_reload ||
        normalized.enable_lua_mod_host) {
        return 3;
    }

    EngineState no_mod_engine{};
    if (!init_engine_state(no_mod_engine, hooks.config)) {
        return 4;
    }
    if (no_mod_engine.app_config.enable_mods ||
        no_mod_engine.app_config.enable_mod_browser ||
        no_mod_engine.app_config.enable_mod_hot_reload ||
        no_mod_engine.app_config.enable_lua_mod_host) {
        cleanup_engine_state(no_mod_engine);
        return 5;
    }
    if (no_mod_engine.menu_manager.find_screen(MenuScreenID::MODS) != nullptr) {
        cleanup_engine_state(no_mod_engine);
        return 6;
    }
    cleanup_engine_state(no_mod_engine);

    EngineState mod_browser_engine{};
    if (!init_engine_state(mod_browser_engine, browser_only)) {
        return 7;
    }
    if (!mod_browser_engine.app_config.enable_mods ||
        !mod_browser_engine.app_config.enable_mod_browser) {
        cleanup_engine_state(mod_browser_engine);
        return 8;
    }
    if (mod_browser_engine.menu_manager.find_screen(MenuScreenID::MODS) == nullptr) {
        cleanup_engine_state(mod_browser_engine);
        return 9;
    }
    cleanup_engine_state(mod_browser_engine);
    return 0;
}
