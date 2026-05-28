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
MenuCommandId g_cmd_join_game = kMenuIdInvalid;

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = action;
    return widget;
}

std::string lobby_status_text(const EngineState& engine) {
    const GubsyLobbyState& lobby = engine.lobby;
    if (!lobby.online)
        return "Offline lobby";

    std::string status;
    if (lobby.is_host) {
        status = lobby.room_code.empty() ? "Currently Direct Hosting"
                                         : "Currently Public Hosting via gubsy-roomd";
    } else {
        status = lobby.room_code.empty() ? "Joined Direct Game" : "Joined Public Game";
    }

    if (!lobby.lobby_name.empty()) {
        status += " | ";
        status += lobby.lobby_name;
    }
    if (!lobby.room_code.empty()) {
        status += " | ";
        status += lobby.room_code;
    }
    if (!lobby.advertised_endpoint.empty()) {
        status += " | ";
        status += lobby.advertised_endpoint;
    }
    status += " | ";
    status += std::to_string(lobby.local_players.size());
    status += " local";
    return status;
}

void command_start_game(MenuContext& ctx, std::int32_t) {
    std::string message;
    if (ctx.engine.lobby.online && !ctx.engine.lobby.is_host) {
        message = "Waiting For Host To Start";
        add_alert(ctx.engine, message);
        ctx.engine.lobby.status_message = message;
        return;
    }
    if (!gubsy_lobby_validate_start(ctx.engine, message)) {
        add_alert(ctx.engine, message);
        ctx.engine.lobby.status_message = message;
        return;
    }
    ctx.engine.lobby.status_message = "Starting game";
    ctx.engine.menu_commands.invoke(ctx, ctx.engine.main_menu_commands.start_game, 0);
}

void command_host_game(MenuContext& ctx, std::int32_t) {
    if (ctx.engine.lobby.online && !ctx.engine.lobby.is_host) {
        add_alert(ctx.engine, "Only the host can host another game");
        return;
    }
    ctx.manager.push_screen(MenuScreenID::LOBBY_HOST_SETUP);
}

void command_join_game(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::LOBBY_JOIN_GAME);
}

BuiltScreen build_shell_lobby(MenuContext& ctx) {
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    static std::vector<std::string> text_cache;
    widgets.clear();
    frame_actions.clear();
    text_cache.clear();
    text_cache.reserve(4);
    gubsy_lobby_ensure_ready(ctx.engine);
    const bool joined_client = ctx.engine.lobby.online && !ctx.engine.lobby.is_host;

    MenuWidget title;
    title.id = 200;
    title.slot = SettingsObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Lobby";
    title.secondary = joined_client ? "Joined room. Waiting for host." : "Session setup";
    widgets.push_back(title);

    text_cache.push_back(lobby_status_text(ctx.engine));
    MenuWidget status;
    status.id = 207;
    status.slot = SettingsObjectID::STATUS;
    status.type = WidgetType::Label;
    status.label = text_cache.back().c_str();
    widgets.push_back(status);

    MenuWidget players;
    players.id = 201;
    players.slot = SettingsObjectID::CARD0;
    players.type = WidgetType::Card;
    text_cache.push_back("Players");
    text_cache.push_back(std::to_string(ctx.engine.lobby.local_players.size()) +
                         " local. Select to manage profiles, binds, and devices.");
    players.label = text_cache[0].c_str();
    players.secondary = text_cache[1].c_str();
    players.on_select = MenuAction::push(MenuScreenID::LOBBY_LOCAL_PLAYERS);

    MenuWidget settings = make_button(202, SettingsObjectID::CARD1, "Game Settings",
                                      MenuAction::push(MenuScreenID::LOBBY_GAME_CONFIG));
    MenuWidget host = make_button(203, SettingsObjectID::CARD2, "Host Game",
                                  joined_client ? MenuAction::none()
                                                : MenuAction::run_command(g_cmd_host_game));
    if (joined_client)
        host.secondary = "Host-only while joined to another lobby.";
    MenuWidget join = make_button(204, SettingsObjectID::CARD3, "Join Game",
                                  MenuAction::run_command(g_cmd_join_game));
    MenuWidget start = make_button(205, SettingsObjectID::ACTION, "Start Game",
                                   joined_client ? MenuAction::none()
                                                 : MenuAction::run_command(g_cmd_start_game));
    if (joined_client)
        start.label = "Waiting For Host";
    MenuWidget back = make_button(206, SettingsObjectID::BACK, "Back", MenuAction::pop());

    players.nav_down = settings.id;
    settings.nav_up = players.id;
    settings.nav_down = host.id;
    host.nav_up = settings.id;
    host.nav_down = join.id;
    join.nav_up = host.id;
    join.nav_down = back.id;
    start.nav_up = join.id;
    start.nav_left = back.id;
    back.nav_up = join.id;
    back.nav_right = start.id;

    widgets.push_back(players);
    widgets.push_back(settings);
    widgets.push_back(host);
    widgets.push_back(join);
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
    g_cmd_start_game = engine.menu_commands.register_command(command_start_game);
    g_cmd_host_game = engine.menu_commands.register_command(command_host_game);
    g_cmd_join_game = engine.menu_commands.register_command(command_join_game);

    MenuScreenDef def;
    def.id = MenuScreenID::SHELL_LOBBY;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_shell_lobby;
    engine.menu_manager.register_screen(def);
}
