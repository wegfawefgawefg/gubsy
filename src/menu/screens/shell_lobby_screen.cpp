#include "src/menu/screens/shell_lobby_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/alerts.hpp"
#include "src/engine_state.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

MenuCommandId g_cmd_start_game = kMenuIdInvalid;
MenuCommandId g_cmd_host_game = kMenuIdInvalid;
MenuCommandId g_cmd_join_game = kMenuIdInvalid;
MenuCommandId g_cmd_leave_session = kMenuIdInvalid;

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = action;
    return widget;
}

int remote_member_count(const GubsyLobbyState& lobby) {
    int count = 0;
    for (const MatchmakingMember& member : lobby.members) {
        if (!lobby.member_id.empty() && member.member_id == lobby.member_id)
            continue;
        ++count;
    }
    return count;
}

int room_player_count(const GubsyLobbyState& lobby) {
    if (!lobby.room_code.empty()) {
        if (lobby.room_current_players > 0)
            return lobby.room_current_players;
        if (!lobby.members.empty())
            return static_cast<int>(lobby.members.size());
    }
    if (lobby.online && !lobby.members.empty()) {
        return static_cast<int>(lobby.local_players.size()) + remote_member_count(lobby);
    }
    return static_cast<int>(lobby.local_players.size());
}

std::string host_display_name(const GubsyLobbyState& lobby) {
    for (const MatchmakingMember& member : lobby.members) {
        if (member.is_host)
            return member.display_name.empty() ? "Host" : member.display_name;
    }
    return "Host";
}

const char* joinability_text(const GubsyLobbyState& lobby) {
    if (session_contract_is_in_game(lobby.contract))
        return "In Game";
    if (!lobby.room_code.empty() && lobby.max_players > 0 &&
        room_player_count(lobby) >= lobby.max_players)
        return "Full";
    return "Lobby Joinable";
}

std::string session_heading(const GubsyLobbyState& lobby) {
    if (!lobby.online)
        return "Offline lobby";
    if (lobby.is_host)
        return lobby.room_code.empty() ? "Currently Direct Hosting"
                                       : "Currently Public Hosting via gubsy-roomd";
    return lobby.room_code.empty() ? "Joined Direct Game" : "Joined Public Game";
}

std::string session_detail_text(const EngineState& engine) {
    const GubsyLobbyState& lobby = engine.lobby;
    if (!lobby.online)
        return "Set up local players, host a session, or join a game.";

    std::string status;
    if (!lobby.lobby_name.empty()) {
        status += lobby.lobby_name;
    }
    if (!lobby.room_code.empty()) {
        if (!status.empty())
            status += " | ";
        status += lobby.room_code;
        if (!lobby.is_host) {
            status += " | Host ";
            status += host_display_name(lobby);
        }
    }
    if (!lobby.advertised_endpoint.empty()) {
        if (!status.empty())
            status += " | ";
        status += lobby.advertised_endpoint;
    }
    if (!status.empty())
        status += " | ";
    status += "Players ";
    status += std::to_string(room_player_count(lobby));
    status += "/";
    status += std::to_string(std::max(1, lobby.max_players));
    status += " | ";
    status += joinability_text(lobby);
    status += " | ";
    status += std::to_string(lobby.local_players.size());
    status += " local";
    const int remote_count = remote_member_count(lobby);
    if (remote_count > 0) {
        status += ", ";
        status += std::to_string(remote_count);
        status += " remote client";
        if (remote_count != 1)
            status += "s";
    }
    return status;
}

void command_start_game(MenuContext& ctx, std::int32_t) {
    std::string message;
    if (ctx.engine.lobby.online && !ctx.engine.lobby.is_host) {
        if (!session_contract_is_in_game(ctx.engine.lobby.contract) ||
            ctx.engine.lobby.contract.realtime_endpoint.empty()) {
            message = "Waiting For Host To Start";
            add_alert(ctx.engine, message, AlertSeverity::Info);
            ctx.engine.lobby.status_message = message;
            return;
        }
        ctx.engine.lobby.status_message = "Entering hosted game";
        ctx.engine.menu_commands.invoke(ctx, ctx.engine.main_menu_commands.start_game, 0);
        return;
    }
    if (!gubsy_lobby_validate_start(ctx.engine, message)) {
        add_alert(ctx.engine, message, AlertSeverity::Error);
        ctx.engine.lobby.status_message = message;
        return;
    }
    if (ctx.engine.lobby.online && ctx.engine.lobby.is_host) {
        ctx.engine.lobby.contract.session_phase = "in_game";
        gubsy_lobby_force_online_tick(ctx.engine);
        add_alert(ctx.engine, "Host started game", AlertSeverity::Success);
    }
    ctx.engine.lobby.status_message = ctx.engine.lobby.online && ctx.engine.lobby.is_host
                                          ? "Host started game"
                                          : "Starting local game";
    ctx.engine.menu_commands.invoke(ctx, ctx.engine.main_menu_commands.start_game, 0);
}

void command_host_game(MenuContext& ctx, std::int32_t) {
    if (ctx.engine.lobby.online && !ctx.engine.lobby.is_host) {
        add_alert(ctx.engine, "Only the host can host another game", AlertSeverity::Warning);
        return;
    }
    ctx.manager.push_screen(MenuScreenID::LOBBY_HOST_SETUP);
}

void command_join_game(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::LOBBY_JOIN_GAME);
}

void command_leave_session(MenuContext& ctx, std::int32_t) {
    std::string message;
    (void)gubsy_lobby_leave_room(ctx.engine, message);
    add_alert(ctx.engine, message, AlertSeverity::Info);
    ctx.engine.lobby.status_message = message;
}

BuiltScreen build_shell_lobby(MenuContext& ctx) {
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    static std::vector<std::string> text_cache;
    widgets.clear();
    frame_actions.clear();
    text_cache.clear();
    text_cache.reserve(8);
    gubsy_lobby_ensure_ready(ctx.engine);
    const bool joined_client = ctx.engine.lobby.online && !ctx.engine.lobby.is_host;
    const bool in_online_session = ctx.engine.lobby.online;
    const bool joined_client_can_play =
        joined_client && session_contract_is_in_game(ctx.engine.lobby.contract) &&
        !ctx.engine.lobby.contract.realtime_endpoint.empty();

    MenuWidget title;
    title.id = 200;
    title.slot = SettingsObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Lobby";
    title.secondary = joined_client
                          ? (joined_client_can_play ? "Joined room. Ready to play."
                                                    : "Joined room. Waiting for host.")
                          : "Session setup";
    widgets.push_back(title);

    text_cache.push_back(session_heading(ctx.engine.lobby));
    const std::size_t status_heading_index = text_cache.size() - 1;
    text_cache.push_back(session_detail_text(ctx.engine));
    const std::size_t status_detail_index = text_cache.size() - 1;
    MenuWidget status;
    status.id = 207;
    status.slot = SettingsObjectID::STATUS;
    status.type = WidgetType::Label;
    status.label = text_cache[status_heading_index].c_str();
    status.secondary = text_cache[status_detail_index].c_str();
    widgets.push_back(status);

    MenuWidget players;
    players.id = 201;
    players.slot = SettingsObjectID::CARD0;
    players.type = WidgetType::Card;
    text_cache.push_back("Players");
    const std::size_t players_label_index = text_cache.size() - 1;
    std::string player_summary = std::to_string(ctx.engine.lobby.local_players.size()) + " local";
    const int remote_count = remote_member_count(ctx.engine.lobby);
    if (remote_count > 0) {
        player_summary += ", ";
        player_summary += std::to_string(remote_count);
        player_summary += " remote client";
        if (remote_count != 1)
            player_summary += "s";
    }
    player_summary += ". Select to manage profiles, binds, devices, and connected players.";
    text_cache.push_back(std::move(player_summary));
    const std::size_t players_summary_index = text_cache.size() - 1;
    players.label = text_cache[players_label_index].c_str();
    players.secondary = text_cache[players_summary_index].c_str();
    players.on_select = MenuAction::push(MenuScreenID::LOBBY_LOCAL_PLAYERS);

    MenuWidget settings = make_button(202, SettingsObjectID::CARD1, "Game Settings",
                                      MenuAction::push(MenuScreenID::LOBBY_GAME_CONFIG));
    MenuWidget host = make_button(203, SettingsObjectID::CARD2, "Host Game",
                                  MenuAction::run_command(g_cmd_host_game));
    MenuWidget join = make_button(204, SettingsObjectID::CARD3, "Join Game",
                                  MenuAction::run_command(g_cmd_join_game));
    const char* start_label = "Start Local Game";
    if (ctx.engine.lobby.online)
        start_label = ctx.engine.lobby.is_host ? "Start Game" : "Waiting For Host";
    if (joined_client_can_play)
        start_label = "Play";
    MenuWidget start = make_button(205, SettingsObjectID::ACTION, start_label,
                                   joined_client && !joined_client_can_play
                                       ? MenuAction::none()
                                       : MenuAction::run_command(g_cmd_start_game));
    if (joined_client) {
        if (joined_client_can_play) {
            start.secondary = "Host state is ready.";
            start.style.bg_r = 30;
            start.style.bg_g = 60;
            start.style.bg_b = 42;
            start.style.focus_r = 120;
            start.style.focus_g = 230;
            start.style.focus_b = 170;
        } else {
            start.secondary = "Only the host can start the game.";
            start.style.bg_r = 30;
            start.style.bg_g = 30;
            start.style.bg_b = 36;
            start.style.fg_r = 150;
            start.style.fg_g = 150;
            start.style.fg_b = 165;
            start.style.focus_r = 120;
            start.style.focus_g = 120;
            start.style.focus_b = 135;
        }
    }
    MenuWidget leave = make_button(208,
                                   SettingsObjectID::CARD4,
                                   ctx.engine.lobby.is_host ? "Stop Hosting" : "Leave Session",
                                   MenuAction::run_command(g_cmd_leave_session));
    if (ctx.engine.lobby.online) {
        leave.secondary = ctx.engine.lobby.is_host ? "Close the hosted session before joining elsewhere."
                                                   : "Disconnect from the current session.";
    }
    MenuWidget back = make_button(206, SettingsObjectID::BACK, "Back", MenuAction::pop());

    players.nav_down = settings.id;
    settings.nav_up = players.id;
    settings.nav_down = in_online_session ? (ctx.engine.lobby.online ? leave.id : back.id) : host.id;
    host.nav_up = settings.id;
    host.nav_down = join.id;
    join.nav_up = host.id;
    join.nav_down = back.id;
    start.nav_up = in_online_session ? leave.id : join.id;
    start.nav_left = back.id;
    leave.nav_up = settings.id;
    leave.nav_down = back.id;
    back.nav_up = ctx.engine.lobby.online ? leave.id : join.id;
    back.nav_right = start.id;

    widgets.push_back(players);
    widgets.push_back(settings);
    if (!ctx.engine.lobby.online) {
        widgets.push_back(host);
        widgets.push_back(join);
    }
    if (ctx.engine.lobby.online)
        widgets.push_back(leave);
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
    g_cmd_leave_session = engine.menu_commands.register_command(command_leave_session);

    MenuScreenDef def;
    def.id = MenuScreenID::SHELL_LOBBY;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_shell_lobby;
    engine.menu_manager.register_screen(def);
}
