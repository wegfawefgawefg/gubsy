#include "src/menu/screens/shell_lobby_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/alerts.hpp"
#include "src/engine_state.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <string>
#include <vector>

namespace {

MenuCommandId g_cmd_start_game = kMenuIdInvalid;
MenuCommandId g_cmd_host_game = kMenuIdInvalid;
MenuCommandId g_cmd_browse_servers = kMenuIdInvalid;

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = action;
    return widget;
}

void command_start_game(MenuContext& ctx, std::int32_t) {
    std::string message;
    if (!gubsy_lobby_validate_start(ctx.engine, message)) {
        add_alert(ctx.engine, message);
        ctx.engine.lobby.status_message = message;
        return;
    }
    ctx.engine.lobby.status_message = "Starting game";
    ctx.engine.menu_commands.invoke(ctx, ctx.engine.main_menu_commands.start_game, 0);
}

void command_host_game(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::LOBBY_HOST_SETUP);
}

void command_browse_servers(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::LOBBY_SERVER_BROWSER);
}

BuiltScreen build_shell_lobby(MenuContext& ctx) {
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    static std::vector<std::string> text_cache;
    widgets.clear();
    frame_actions.clear();
    text_cache.clear();
    gubsy_lobby_ensure_ready(ctx.engine);

    MenuWidget title;
    title.id = 200;
    title.slot = SettingsObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Lobby";
    title.secondary = "Session setup";
    widgets.push_back(title);

    MenuWidget players;
    players.id = 201;
    players.slot = SettingsObjectID::CARD0;
    players.type = WidgetType::Card;
    text_cache.push_back("Local Players");
    text_cache.push_back(std::to_string(ctx.engine.lobby.local_players.size()) +
                         " joined. Select to manage profiles, binds, and devices.");
    players.label = text_cache[0].c_str();
    players.secondary = text_cache[1].c_str();
    players.on_select = MenuAction::push(MenuScreenID::LOBBY_LOCAL_PLAYERS);

    MenuWidget settings = make_button(202,
                                      SettingsObjectID::CARD1,
                                      "Game / Settings",
                                      MenuAction::push(MenuScreenID::SETTINGS));
    MenuWidget host = make_button(203,
                                  SettingsObjectID::CARD2,
                                  "Host Game",
                                  MenuAction::run_command(g_cmd_host_game));
    MenuWidget browse = make_button(204,
                                    SettingsObjectID::CARD3,
                                    "Browse Servers",
                                    MenuAction::run_command(g_cmd_browse_servers));
    MenuWidget start = make_button(205,
                                   SettingsObjectID::PREV,
                                   "Start Game",
                                   MenuAction::run_command(g_cmd_start_game));
    MenuWidget back = make_button(206, SettingsObjectID::BACK, "Back", MenuAction::pop());

    players.nav_down = settings.id;
    settings.nav_up = players.id;
    settings.nav_down = host.id;
    host.nav_up = settings.id;
    host.nav_down = browse.id;
    browse.nav_up = host.id;
    browse.nav_down = start.id;
    start.nav_up = browse.id;
    start.nav_down = back.id;
    back.nav_up = start.id;

    widgets.push_back(players);
    widgets.push_back(settings);
    widgets.push_back(host);
    widgets.push_back(browse);
    widgets.push_back(start);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.frame_actions = MenuActionList{frame_actions};
    built.default_focus = players.id;
    return built;
}

} // namespace

void register_shell_lobby_screen(EngineState& engine) {
    if (g_cmd_start_game == kMenuIdInvalid)
        g_cmd_start_game = engine.menu_commands.register_command(command_start_game);
    if (g_cmd_host_game == kMenuIdInvalid)
        g_cmd_host_game = engine.menu_commands.register_command(command_host_game);
    if (g_cmd_browse_servers == kMenuIdInvalid)
        g_cmd_browse_servers = engine.menu_commands.register_command(command_browse_servers);

    MenuScreenDef def;
    def.id = MenuScreenID::SHELL_LOBBY;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_shell_lobby;
    engine.menu_manager.register_screen(def);
}
