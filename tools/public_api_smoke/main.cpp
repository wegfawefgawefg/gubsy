#include <gubsy/app.hpp>
#include <gubsy/input/binds.hpp>
#include <gubsy/layout/layout.hpp>
#include <gubsy/lobby/session.hpp>
#include <gubsy/lobby/steam.hpp>
#include <gubsy/menu/menu.hpp>
#include <gubsy/profiles/profiles.hpp>
#include <gubsy/run.hpp>
#include <gubsy/runtime.hpp>
#include <gubsy/settings/settings.hpp>

namespace {

GubsyLobbyJoinResult public_api_join_transport(void*, const GubsyLobbyState&, const char*,
                                               std::uint16_t) {
    GubsyLobbyJoinResult result;
    result.ok = true;
    result.status = "joined";
    return result;
}

} // namespace

int main() {
    GubsySteamBackendStatus steam_status = gubsy_steam_backend_status();
#if GUB_ENABLE_STEAM
    if (!steam_status.compiled || steam_status.sdk_ready || steam_status.message == nullptr) {
        return 32;
    }
#else
    if (steam_status.compiled || steam_status.sdk_ready || steam_status.message == nullptr) {
        return 32;
    }
#endif

    GubsyAppHooks hooks{};
    if (hooks.config.enable_mods || hooks.config.enable_mod_browser ||
        hooks.config.enable_mod_hot_reload || hooks.config.enable_lua_mod_host) {
        return 1;
    }

    GubsyAppConfig browser_only{};
    browser_only.enable_mod_browser = true;
    GubsyAppConfig normalized = normalize_gubsy_app_config(browser_only);
    if (!normalized.enable_mods || !normalized.enable_mod_browser) {
        return 2;
    }

    GubsyAppConfig no_mods{};
    no_mods.data_root = "data";
    normalized = normalize_gubsy_app_config(no_mods);
    if (normalized.enable_mods || normalized.enable_mod_browser ||
        normalized.enable_mod_hot_reload || normalized.enable_lua_mod_host) {
        return 3;
    }

    GubsyRuntime no_mod_engine{};
    if (!init_gubsy_runtime(no_mod_engine, no_mods)) {
        return 4;
    }
    const GubsyAppConfig& no_mod_config = gubsy_runtime_config(no_mod_engine);
    if (no_mod_config.enable_mods || no_mod_config.enable_mod_browser ||
        no_mod_config.enable_mod_hot_reload || no_mod_config.enable_lua_mod_host) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 5;
    }
    if (no_mod_config.data_root != "data") {
        cleanup_gubsy_runtime(no_mod_engine);
        return 14;
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
    if (!gubsy_runtime_has_menu_screen(no_mod_engine, MenuScreenID::IN_GAME_MENU)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 26;
    }
    if (gubsy_add_lobby_local_player(no_mod_engine) != 1) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 18;
    }
    const GubsyLobbyState& initial_lobby = gubsy_get_lobby_state(no_mod_engine);
    const GubsyLobbyPlayer initial_player = initial_lobby.local_players.front();
    if (!gubsy_select_lobby_local_player(no_mod_engine, 1)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 19;
    }
    if (!gubsy_set_lobby_player_user_profile(no_mod_engine, 1, initial_player.user_profile_id)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 20;
    }
    if (!gubsy_set_lobby_player_binds_profile(no_mod_engine, 1, initial_player.binds_profile_id)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 21;
    }
    if (!gubsy_set_lobby_player_input_settings_profile(no_mod_engine, 1,
                                                       initial_player.input_settings_profile_id)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 22;
    }
    GubsyLobbyDeviceAssignment mouse_device{InputSourceType::Mouse, 0};
    gubsy_toggle_lobby_player_device(no_mod_engine, 1, mouse_device);
    if (gubsy_get_lobby_state(no_mod_engine).local_players[1].devices.empty()) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 23;
    }
    if (!gubsy_remove_lobby_local_player(no_mod_engine, 1)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 24;
    }
    if (gubsy_remove_lobby_local_player(no_mod_engine, 0)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 25;
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
    struct PauseSmoke {
        GubsyRuntime* runtime{nullptr};
        bool resumed{false};
    } pause_smoke{&no_mod_engine, false};
    MenuCommandId resume_command = gubsy_register_menu_command(
        no_mod_engine,
        [](void* user_data, std::int32_t) {
            auto* smoke = static_cast<PauseSmoke*>(user_data);
            smoke->resumed = true;
            gubsy_close_in_game_menu(*smoke->runtime);
        },
        &pause_smoke);
    if (resume_command == kMenuIdInvalid) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 27;
    }
    GubsyInGameMenuCommands in_game_commands{};
    in_game_commands.resume = resume_command;
    gubsy_set_in_game_menu_commands(no_mod_engine, in_game_commands);
    if (!gubsy_open_in_game_menu(no_mod_engine) || !gubsy_in_game_menu_open(no_mod_engine)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 28;
    }
    if (!gubsy_push_menu_screen(no_mod_engine, MenuScreenID::SETTINGS)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 30;
    }
    gubsy_pop_menu_screen(no_mod_engine);
    if (!gubsy_in_game_menu_open(no_mod_engine)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 31;
    }
    MenuInputState pause_input{};
    pause_input.select = true;
    gubsy_set_menu_input(no_mod_engine, pause_input);
    gubsy_update_menu(no_mod_engine, 0.016f, 1280, 720);
    if (!pause_smoke.resumed || gubsy_in_game_menu_open(no_mod_engine)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 29;
    }
    bool lobby_start_called = false;
    MenuCommandId lobby_start = gubsy_register_menu_command(
        no_mod_engine, [](void* user_data, std::int32_t) { *static_cast<bool*>(user_data) = true; },
        &lobby_start_called);
    if (lobby_start == kMenuIdInvalid) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 15;
    }
    commands.start_game = lobby_start;
    gubsy_set_main_menu_commands(no_mod_engine, commands);
    std::string lobby_start_message;
    if (!gubsy_start_lobby_game(no_mod_engine, lobby_start_message) || !lobby_start_called) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 16;
    }
    if (lobby_start_message != "Starting game") {
        cleanup_gubsy_runtime(no_mod_engine);
        return 17;
    }
    GubsyLobbyCommands lobby_commands{};
    lobby_commands.join = public_api_join_transport;
    gubsy_set_lobby_commands(no_mod_engine, lobby_commands);
    if (!gubsy_join_lobby_direct(no_mod_engine, "127.0.0.1", 35355, lobby_start_message)) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 33;
    }
    lobby_start_called = false;
    if (gubsy_start_lobby_game(no_mod_engine, lobby_start_message) || lobby_start_called) {
        cleanup_gubsy_runtime(no_mod_engine);
        return 34;
    }
    if (lobby_start_message != "Waiting For Host To Start") {
        cleanup_gubsy_runtime(no_mod_engine);
        return 35;
    }
    cleanup_gubsy_runtime(no_mod_engine);

    GubsyRuntime mod_browser_engine{};
    if (!init_gubsy_runtime(mod_browser_engine, browser_only)) {
        return 7;
    }
    const GubsyAppConfig& mod_browser_config = gubsy_runtime_config(mod_browser_engine);
    if (!mod_browser_config.enable_mods || !mod_browser_config.enable_mod_browser) {
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
