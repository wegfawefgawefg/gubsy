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
    EngineState engine{};
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
    if (!init_engine_state(engine, hooks.config)) {
        return 4;
    }
    if (engine.app_config.enable_mods ||
        engine.app_config.enable_mod_browser ||
        engine.app_config.enable_mod_hot_reload ||
        engine.app_config.enable_lua_mod_host) {
        cleanup_engine_state(engine);
        return 5;
    }
    cleanup_engine_state(engine);
    return 0;
}
