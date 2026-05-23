#include <gubsy/app.hpp>
#include <gubsy/input/binds.hpp>
#include <gubsy/layout/layout.hpp>
#include <gubsy/lobby/session.hpp>
#include <gubsy/menu/menu.hpp>
#include <gubsy/profiles/profiles.hpp>
#include <gubsy/run.hpp>
#include <gubsy/runtime.hpp>
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

    GubsyRuntime no_mod_engine{};
    if (!init_gubsy_runtime(no_mod_engine, hooks.config)) {
        return 4;
    }
    const GubsyAppConfig& no_mod_config = gubsy_runtime_config(no_mod_engine);
    if (no_mod_config.enable_mods ||
        no_mod_config.enable_mod_browser ||
        no_mod_config.enable_mod_hot_reload ||
        no_mod_config.enable_lua_mod_host) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 5;
    }
    if (gubsy_runtime_has_menu_screen(no_mod_engine, MenuScreenID::MODS)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 6;
    }
    if (!gubsy_runtime_has_menu_screen(no_mod_engine, MenuScreenID::SHELL_MAIN)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 10;
    }
    if (!gubsy_runtime_has_menu_screen(no_mod_engine, MenuScreenID::SHELL_LOBBY)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 13;
    }
    bool command_called = false;
    MenuCommandId command = gubsy_register_menu_command(
        no_mod_engine,
        [](void* user_data, std::int32_t payload) {
            if (payload == 7)
                *static_cast<bool*>(user_data) = true;
        },
        &command_called);
    if (command == kMenuIdInvalid) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 11;
    }
    GubsyMainMenuCommands commands{};
    commands.start_game = command;
    gubsy_set_main_menu_commands(no_mod_engine, commands);
    if (!gubsy_show_main_menu(no_mod_engine)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 12;
    }
    cleanup_gubsy_runtime(no_mod_engine);

    GubsyRuntime mod_browser_engine{};
    if (!init_gubsy_runtime(mod_browser_engine, browser_only)) {
        return 7;
    }
    const GubsyAppConfig& mod_browser_config = gubsy_runtime_config(mod_browser_engine);
    if (!mod_browser_config.enable_mods ||
        !mod_browser_config.enable_mod_browser) {
        cleanup_gubsy_runtime(mod_browser_engine);
        return 8;
    }
    if (!gubsy_runtime_has_menu_screen(mod_browser_engine, MenuScreenID::MODS)) {
        cleanup_gubsy_runtime(mod_browser_engine);
        return 9;
    }
    cleanup_gubsy_runtime(mod_browser_engine);
    return 0;
}
