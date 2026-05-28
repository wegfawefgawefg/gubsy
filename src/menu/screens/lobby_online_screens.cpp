#include "src/menu/screens/lobby_online_screens.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/alerts.hpp"
#include "src/engine_state.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr WidgetId kTitleWidgetId = 2700;
constexpr WidgetId kStatusWidgetId = 2701;
constexpr WidgetId kPageWidgetId = 2702;
constexpr WidgetId kHostInputWidgetId = 2720;
constexpr WidgetId kPortInputWidgetId = 2721;
constexpr WidgetId kActionWidgetId = 2723;
constexpr WidgetId kRefreshWidgetId = 2724;
constexpr WidgetId kFirstRoomWidgetId = 2725;
constexpr WidgetId kJoinByIpWidgetId = 2726;
constexpr WidgetId kBrowseServersWidgetId = 2727;
constexpr WidgetId kMaxPlayersWidgetId = 2732;
constexpr WidgetId kPrevWidgetId = 2728;
constexpr WidgetId kNextWidgetId = 2729;
constexpr WidgetId kBackWidgetId = 2730;
constexpr int kDefaultPort = 35355;
constexpr int kRoomsPerPage = 2;

MenuCommandId g_cmd_host_direct = kMenuIdInvalid;
MenuCommandId g_cmd_publish_room = kMenuIdInvalid;
MenuCommandId g_cmd_leave = kMenuIdInvalid;
MenuCommandId g_cmd_join_direct = kMenuIdInvalid;
MenuCommandId g_cmd_join_code = kMenuIdInvalid;
MenuCommandId g_cmd_join_listed = kMenuIdInvalid;
MenuCommandId g_cmd_refresh = kMenuIdInvalid;
MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_max_players_delta = kMenuIdInvalid;

struct OnlineState {
    std::string host_text{"127.0.0.1"};
    std::string port_text{std::to_string(kDefaultPort)};
    std::string room_code_text;
    std::string status_text;
    std::string page_text;
    int page{0};
    int total_pages{1};
    bool initialized{false};
};

MenuWidget make_label(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Label;
    widget.label = label;
    return widget;
}

MenuWidget make_text(WidgetId id, UILayoutObjectId slot, const char* label, std::string* buffer,
                     int max_len) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::TextInput;
    widget.label = label;
    widget.text_buffer = buffer;
    widget.text_max_len = max_len;
    return widget;
}

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = action;
    return widget;
}

MenuWidget make_option(WidgetId id, UILayoutObjectId slot, const char* label, const char* badge,
                       MenuAction left, MenuAction right) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::OptionCycle;
    widget.label = label;
    widget.badge = badge;
    widget.on_left = left;
    widget.on_right = right;
    return widget;
}

std::uint16_t parse_port(const std::string& text) {
    try {
        int value = std::stoi(text);
        if (value <= 0 || value > 65535)
            return static_cast<std::uint16_t>(kDefaultPort);
        return static_cast<std::uint16_t>(value);
    } catch (...) {
        return static_cast<std::uint16_t>(kDefaultPort);
    }
}

void sync_state_from_lobby(OnlineState& st, const GubsyLobbyState& lobby) {
    if (st.initialized)
        return;
    st.host_text = lobby.join_host.empty() ? "127.0.0.1" : lobby.join_host;
    int port = lobby.network_port > 0 ? lobby.network_port : kDefaultPort;
    st.port_text = std::to_string(port);
    st.room_code_text = lobby.room_code;
    st.initialized = true;
}

void update_page(OnlineState& st, int room_count) {
    st.total_pages = std::max(1, (room_count + kRoomsPerPage - 1) / kRoomsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

bool validate_common(MenuContext& ctx) {
    std::string message;
    if (gubsy_lobby_validate_start(ctx.engine, message))
        return true;
    ctx.engine.lobby.status_message = message;
    add_alert(ctx.engine, message);
    return false;
}

void command_host_direct(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<OnlineState>();
    if (!validate_common(ctx))
        return;

    ctx.engine.lobby.visibility = GubsyLobbyVisibility::Private;
    std::uint16_t port = parse_port(st.port_text);
    std::string message;
    bool ok = gubsy_lobby_host_direct(ctx.engine, port, message);
    add_alert(ctx.engine, message);
    if (ok)
        ctx.manager.pop_screen();
}

void command_publish_room(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<OnlineState>();
    if (!validate_common(ctx))
        return;

    ctx.engine.lobby.visibility = GubsyLobbyVisibility::Public;
    std::uint16_t port = parse_port(st.port_text);
    std::string message;
    bool ok = gubsy_lobby_host_room(ctx.engine, port, message);
    add_alert(ctx.engine, message);
    if (ok)
        ctx.manager.pop_screen();
}

void command_leave(MenuContext& ctx, std::int32_t) {
    std::string message;
    (void)gubsy_lobby_leave_room(ctx.engine, message);
    add_alert(ctx.engine, message);
}

void command_join_direct(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<OnlineState>();
    if (!validate_common(ctx))
        return;

    std::string message;
    bool ok = gubsy_lobby_join_direct(ctx.engine, st.host_text, parse_port(st.port_text), message);
    add_alert(ctx.engine, message);
    if (ok)
        ctx.manager.pop_screen();
}

void command_join_code(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<OnlineState>();
    std::string message;
    bool ok = gubsy_lobby_join_room_code(ctx.engine, st.room_code_text, message);
    add_alert(ctx.engine, message);
    if (ok)
        ctx.manager.pop_screen();
}

void command_join_listed(MenuContext& ctx, std::int32_t index) {
    if (index < 0 || index >= static_cast<int>(ctx.engine.lobby.discovered_rooms.size()))
        return;
    std::string message;
    const MatchmakingRoom& room =
        ctx.engine.lobby.discovered_rooms[static_cast<std::size_t>(index)];
    bool ok = gubsy_lobby_join_room(ctx.engine, room, message);
    add_alert(ctx.engine, message);
    if (ok)
        ctx.manager.pop_screen();
}

void command_refresh(MenuContext& ctx, std::int32_t) {
    std::string message;
    (void)gubsy_lobby_refresh_rooms(ctx.engine, true, message);
    add_alert(ctx.engine, message);
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<OnlineState>();
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st, static_cast<int>(ctx.engine.lobby.discovered_rooms.size()));
}

void command_max_players_delta(MenuContext& ctx, std::int32_t delta) {
    ctx.engine.lobby.max_players = std::clamp(ctx.engine.lobby.max_players + delta, 1, 32);
}

bool room_is_joinable(const MatchmakingRoom& room) {
    if (session_contract_is_in_game(room.contract))
        return false;
    return room.max_players <= 0 || room.current_players < room.max_players;
}

std::string room_card_detail(const MatchmakingRoom& room) {
    std::string detail = "Host: ";
    detail += room.host_name.empty() ? "Unknown" : room.host_name;
    detail += " | Players ";
    detail += std::to_string(room.current_players);
    detail += "/";
    detail += std::to_string(room.max_players);
    if (session_contract_is_in_game(room.contract))
        detail += " | In Game | Unavailable";
    else if (!room_is_joinable(room))
        detail += " | Full | Unavailable";
    else
        detail += " | Lobby | Joinable";
    detail += " | gubsy-roomd";
    if (!room.contract.realtime_endpoint.empty()) {
        detail += " | ";
        detail += room.contract.realtime_endpoint;
    }
    return detail;
}

BuiltScreen build_host_screen(MenuContext& ctx) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<OnlineState>();
    sync_state_from_lobby(st, ctx.engine.lobby);

    static std::vector<MenuWidget> widgets;
    static std::string max_players_text;
    widgets.clear();

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Host Session"));
    st.status_text = ctx.engine.lobby.status_message.empty() ? "Network session setup"
                                                             : ctx.engine.lobby.status_message;
    widgets.push_back(
        make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));

    MenuWidget lobby_name = make_text(kHostInputWidgetId, SettingsObjectID::CARD0, "Room Name",
                                      &ctx.engine.lobby.lobby_name, 48);
    lobby_name.placeholder = "Room Name";
    MenuWidget port_input =
        make_text(kPortInputWidgetId, SettingsObjectID::CARD1, "Host Port", &st.port_text, 6);
    port_input.placeholder = "35355";

    max_players_text = std::to_string(std::clamp(ctx.engine.lobby.max_players, 1, 32));
    MenuWidget max_players =
        make_option(kMaxPlayersWidgetId, SettingsObjectID::CARD2, "Max Players",
                    max_players_text.c_str(), MenuAction::run_command(g_cmd_max_players_delta, -1),
                    MenuAction::run_command(g_cmd_max_players_delta, 1));
    max_players.secondary = "Session-wide player cap advertised to the room backend.";

    MenuWidget publish =
        make_button(kRefreshWidgetId, SettingsObjectID::CARD3, "Host Public",
                    MenuAction::run_command(g_cmd_publish_room));
    publish.secondary = "Starts hosting and lists this game on the configured room server.";

    MenuWidget action = make_button(
        kActionWidgetId, SettingsObjectID::ACTION,
        ctx.engine.lobby.online ? "Leave Session" : "Host Direct",
        MenuAction::run_command(ctx.engine.lobby.online ? g_cmd_leave : g_cmd_host_direct));
    if (!ctx.engine.lobby.online)
        action.secondary = "Starts direct/private hosting without listing this game.";
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());

    lobby_name.nav_down = port_input.id;
    port_input.nav_up = lobby_name.id;
    port_input.nav_down = max_players.id;
    max_players.nav_up = port_input.id;
    max_players.nav_down = publish.id;
    publish.nav_up = max_players.id;
    publish.nav_down = back.id;
    action.nav_up = publish.id;
    action.nav_left = back.id;
    back.nav_up = publish.id;
    back.nav_right = action.id;
    widgets.push_back(lobby_name);
    widgets.push_back(port_input);
    widgets.push_back(max_players);
    widgets.push_back(publish);
    widgets.push_back(action);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = lobby_name.id;
    return built;
}

BuiltScreen build_join_menu_screen(MenuContext&) {
    static std::vector<MenuWidget> widgets;
    widgets.clear();

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Join Game"));
    widgets.push_back(make_label(kStatusWidgetId, SettingsObjectID::STATUS,
                                 "Choose how to connect to a hosted game"));

    MenuWidget join_by_ip = make_button(kJoinByIpWidgetId, SettingsObjectID::CARD0, "Join By IP",
                                        MenuAction::push(MenuScreenID::LOBBY_JOIN_BY_IP));
    join_by_ip.secondary = "Connect directly to a host address and port.";
    MenuWidget browse = make_button(kBrowseServersWidgetId, SettingsObjectID::CARD1,
                                    "Browse Servers",
                                    MenuAction::push(MenuScreenID::LOBBY_SERVER_BROWSER));
    browse.secondary = "Find public rooms listed by the configured room server.";
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());

    join_by_ip.nav_down = browse.id;
    browse.nav_up = join_by_ip.id;
    browse.nav_down = back.id;
    back.nav_up = browse.id;

    widgets.push_back(join_by_ip);
    widgets.push_back(browse);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = join_by_ip.id;
    return built;
}

BuiltScreen build_join_by_ip_screen(MenuContext& ctx) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<OnlineState>();
    sync_state_from_lobby(st, ctx.engine.lobby);

    static std::vector<MenuWidget> widgets;
    widgets.clear();

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Join By IP"));
    st.status_text = ctx.engine.lobby.last_error.empty() ? ctx.engine.lobby.status_message
                                                         : ctx.engine.lobby.last_error;
    if (st.status_text.empty())
        st.status_text = "Direct network join";
    widgets.push_back(
        make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));

    MenuWidget host = make_text(kHostInputWidgetId, SettingsObjectID::CARD0, "IP / Host",
                                &st.host_text, 64);
    host.placeholder = "192.168.1.10";
    MenuWidget port =
        make_text(kPortInputWidgetId, SettingsObjectID::CARD1, "Port", &st.port_text, 6);
    port.placeholder = "35355";
    MenuWidget action = make_button(kActionWidgetId, SettingsObjectID::ACTION, "Join",
                                    MenuAction::run_command(g_cmd_join_direct));
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());

    host.nav_down = port.id;
    port.nav_up = host.id;
    port.nav_down = action.id;
    action.nav_up = port.id;
    action.nav_left = back.id;
    back.nav_up = port.id;
    back.nav_right = action.id;

    widgets.push_back(host);
    widgets.push_back(port);
    widgets.push_back(action);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = host.id;
    return built;
}

BuiltScreen build_browser_screen(MenuContext& ctx) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<OnlineState>();
    sync_state_from_lobby(st, ctx.engine.lobby);
    std::string refresh_message;
    (void)gubsy_lobby_refresh_rooms(ctx.engine, false, refresh_message);
    update_page(st, static_cast<int>(ctx.engine.lobby.discovered_rooms.size()));

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(static_cast<std::size_t>(kRoomsPerPage) * 2 + 2);

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Browse Servers"));
    st.status_text = ctx.engine.lobby.last_error.empty() ? ctx.engine.lobby.status_message
                                                         : ctx.engine.lobby.last_error;
    if (st.status_text.empty())
        st.status_text = "Public room browser";
    widgets.push_back(
        make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));
    widgets.push_back(make_label(kPageWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuWidget prev = make_button(kPrevWidgetId, SettingsObjectID::PREV, "<",
                                  st.page > 0 ? MenuAction::run_command(g_cmd_page_delta, -1)
                                              : MenuAction::none());
    prev.role = MenuWidgetRole::PagePrev;
    MenuWidget next =
        make_button(kNextWidgetId, SettingsObjectID::NEXT, ">",
                    st.page + 1 < st.total_pages ? MenuAction::run_command(g_cmd_page_delta, 1)
                                                 : MenuAction::none());
    next.role = MenuWidgetRole::PageNext;
    widgets.push_back(prev);
    widgets.push_back(next);

    std::vector<WidgetId> room_ids;
    int start = st.page * kRoomsPerPage;
    const bool no_rooms = ctx.engine.lobby.discovered_rooms.empty();
    for (int i = 0; i < kRoomsPerPage; ++i) {
        int room_index = start + i;
        WidgetId widget_id = kFirstRoomWidgetId + static_cast<WidgetId>(i);
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        if (no_rooms && i == 0) {
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            card.label = "No Public Games";
            card.secondary = "Refresh to check the room server again.";
            widgets.push_back(card);
        } else if (room_index < static_cast<int>(ctx.engine.lobby.discovered_rooms.size())) {
            const MatchmakingRoom& room =
                ctx.engine.lobby.discovered_rooms[static_cast<std::size_t>(room_index)];
            text_cache.push_back(room.session_name.empty() ? room.room_code : room.session_name);
            text_cache.push_back(room_card_detail(room));

            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            card.label = text_cache[text_cache.size() - 2].c_str();
            card.secondary = text_cache[text_cache.size() - 1].c_str();
            card.badge = room.room_code.c_str();
            if (room_is_joinable(room)) {
                card.on_select = MenuAction::run_command(g_cmd_join_listed, room_index);
            } else {
                card.style.bg_r = 32;
                card.style.bg_g = 30;
                card.style.bg_b = 34;
                card.style.focus_r = 120;
                card.style.focus_g = 100;
                card.style.focus_b = 120;
            }
            widgets.push_back(card);
            room_ids.push_back(card.id);
        } else {
            widgets.push_back(make_label(widget_id, slot, ""));
        }
    }

    MenuWidget refresh = make_button(kRefreshWidgetId, SettingsObjectID::SEARCH, "Refresh",
                                     MenuAction::run_command(g_cmd_refresh));
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(refresh);
    std::size_t refresh_idx = widgets.size() - 1;
    widgets.push_back(back);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& refresh_ref = widgets[refresh_idx];
    MenuWidget& back_ref = widgets[back_idx];

    for (std::size_t i = 0; i < room_ids.size(); ++i) {
        for (MenuWidget& widget : widgets) {
            if (widget.id != room_ids[i])
                continue;
            widget.nav_up = (i == 0) ? refresh_ref.id : room_ids[i - 1];
            widget.nav_down = (i + 1 < room_ids.size()) ? room_ids[i + 1] : refresh_ref.id;
            break;
        }
    }
    refresh_ref.nav_up = room_ids.empty() ? back_ref.id : room_ids.back();
    refresh_ref.nav_down = back_ref.id;
    back_ref.nav_up = refresh_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = room_ids.empty() ? refresh_ref.id : room_ids.front();
    return built;
}

void register_screen(EngineState& engine, MenuScreenId id, MenuBuildFn build) {
    MenuScreenDef def;
    def.id = id;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<OnlineState>();
    def.build = build;
    engine.menu_manager.register_screen(def);
}

} // namespace

void register_lobby_online_screens(EngineState& engine) {
    g_cmd_host_direct = engine.menu_commands.register_command(command_host_direct);
    g_cmd_publish_room = engine.menu_commands.register_command(command_publish_room);
    g_cmd_leave = engine.menu_commands.register_command(command_leave);
    g_cmd_join_direct = engine.menu_commands.register_command(command_join_direct);
    g_cmd_join_code = engine.menu_commands.register_command(command_join_code);
    g_cmd_join_listed = engine.menu_commands.register_command(command_join_listed);
    g_cmd_refresh = engine.menu_commands.register_command(command_refresh);
    g_cmd_page_delta = engine.menu_commands.register_command(command_page_delta);
    g_cmd_max_players_delta = engine.menu_commands.register_command(command_max_players_delta);

    register_screen(engine, MenuScreenID::LOBBY_HOST_SETUP, build_host_screen);
    register_screen(engine, MenuScreenID::LOBBY_JOIN_GAME, build_join_menu_screen);
    register_screen(engine, MenuScreenID::LOBBY_JOIN_BY_IP, build_join_by_ip_screen);
    register_screen(engine, MenuScreenID::LOBBY_SERVER_BROWSER, build_browser_screen);
}
