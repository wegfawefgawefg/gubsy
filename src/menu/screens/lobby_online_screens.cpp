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
constexpr WidgetId kCodeInputWidgetId = 2722;
constexpr WidgetId kActionWidgetId = 2723;
constexpr WidgetId kRefreshWidgetId = 2724;
constexpr WidgetId kFirstRoomWidgetId = 2725;
constexpr WidgetId kVisibilityWidgetId = 2731;
constexpr WidgetId kMaxPlayersWidgetId = 2732;
constexpr WidgetId kPrevWidgetId = 2728;
constexpr WidgetId kNextWidgetId = 2729;
constexpr WidgetId kBackWidgetId = 2730;
constexpr int kDefaultPort = 35355;
constexpr int kRoomsPerPage = 2;

MenuCommandId g_cmd_host = kMenuIdInvalid;
MenuCommandId g_cmd_leave = kMenuIdInvalid;
MenuCommandId g_cmd_join_code = kMenuIdInvalid;
MenuCommandId g_cmd_join_listed = kMenuIdInvalid;
MenuCommandId g_cmd_refresh = kMenuIdInvalid;
MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_visibility_delta = kMenuIdInvalid;
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

MenuWidget make_text(WidgetId id,
                     UILayoutObjectId slot,
                     const char* label,
                     std::string* buffer,
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

MenuWidget make_option(WidgetId id,
                       UILayoutObjectId slot,
                       const char* label,
                       const char* badge,
                       MenuAction left,
                       MenuAction right) {
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
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " +
                   std::to_string(st.total_pages);
}

bool validate_common(MenuContext& ctx) {
    std::string message;
    if (gubsy_lobby_validate_start(ctx.engine, message))
        return true;
    ctx.engine.lobby.status_message = message;
    add_alert(ctx.engine, message);
    return false;
}

void command_host(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<OnlineState>();
    if (!validate_common(ctx))
        return;

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

void command_visibility_delta(MenuContext& ctx, std::int32_t) {
    ctx.engine.lobby.visibility =
        ctx.engine.lobby.visibility == GubsyLobbyVisibility::Public
            ? GubsyLobbyVisibility::Private
            : GubsyLobbyVisibility::Public;
}

void command_max_players_delta(MenuContext& ctx, std::int32_t delta) {
    ctx.engine.lobby.max_players = std::clamp(ctx.engine.lobby.max_players + delta, 1, 32);
}

BuiltScreen build_host_screen(MenuContext& ctx) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<OnlineState>();
    sync_state_from_lobby(st, ctx.engine.lobby);

    static std::vector<MenuWidget> widgets;
    static std::string max_players_text;
    widgets.clear();

    widgets.push_back(make_label(kTitleWidgetId,
                                 SettingsObjectID::TITLE,
                                 "Host Session"));
    st.status_text = ctx.engine.lobby.status_message.empty() ? "Network session setup"
                                                             : ctx.engine.lobby.status_message;
    widgets.push_back(make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));

    MenuWidget lobby_name = make_text(kHostInputWidgetId,
                                      SettingsObjectID::CARD0,
                                      "Lobby Name",
                                      &ctx.engine.lobby.lobby_name,
                                      48);
    lobby_name.placeholder = "Local Game";
    MenuWidget port_input = make_text(kPortInputWidgetId,
                                      SettingsObjectID::CARD1,
                                      "Port",
                                      &st.port_text,
                                      6);
    port_input.placeholder = "35355";

    const char* visibility_text =
        ctx.engine.lobby.visibility == GubsyLobbyVisibility::Public ? "Public" : "Private";
    MenuAction visibility_delta = MenuAction::run_command(g_cmd_visibility_delta);
    MenuWidget visibility = make_option(kVisibilityWidgetId,
                                        SettingsObjectID::CARD2,
                                        "Visibility",
                                        visibility_text,
                                        visibility_delta,
                                        visibility_delta);
    visibility.secondary = "Advertise the room when the backend supports room visibility.";

    max_players_text = std::to_string(std::clamp(ctx.engine.lobby.max_players, 1, 32));
    MenuWidget max_players = make_option(kMaxPlayersWidgetId,
                                         SettingsObjectID::CARD3,
                                         "Max Players",
                                         max_players_text.c_str(),
                                         MenuAction::run_command(g_cmd_max_players_delta, -1),
                                         MenuAction::run_command(g_cmd_max_players_delta, 1));
    max_players.secondary = "Session-wide player cap advertised to the room backend.";

    MenuWidget action = make_button(kActionWidgetId,
                                    SettingsObjectID::ACTION,
                                    ctx.engine.lobby.online ? "Leave Room" : "Host Room",
                                    MenuAction::run_command(ctx.engine.lobby.online ? g_cmd_leave
                                                                                   : g_cmd_host));
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());

    lobby_name.nav_down = port_input.id;
    port_input.nav_up = lobby_name.id;
    port_input.nav_down = visibility.id;
    visibility.nav_up = port_input.id;
    visibility.nav_down = max_players.id;
    max_players.nav_up = visibility.id;
    max_players.nav_down = back.id;
    action.nav_up = max_players.id;
    action.nav_left = back.id;
    back.nav_up = max_players.id;
    back.nav_right = action.id;
    widgets.push_back(lobby_name);
    widgets.push_back(port_input);
    widgets.push_back(visibility);
    widgets.push_back(max_players);
    widgets.push_back(action);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = lobby_name.id;
    return built;
}

BuiltScreen build_join_screen(MenuContext& ctx) {
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

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Browse Servers"));
    st.status_text = ctx.engine.lobby.last_error.empty() ? ctx.engine.lobby.status_message
                                                         : ctx.engine.lobby.last_error;
    if (st.status_text.empty())
        st.status_text = "Room browser";
    widgets.push_back(make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));
    widgets.push_back(make_label(kPageWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuWidget prev = make_button(kPrevWidgetId,
                                  SettingsObjectID::PREV,
                                  "<",
                                  st.page > 0 ? MenuAction::run_command(g_cmd_page_delta, -1)
                                              : MenuAction::none());
    prev.role = MenuWidgetRole::PagePrev;
    MenuWidget next = make_button(kNextWidgetId,
                                  SettingsObjectID::NEXT,
                                  ">",
                                  st.page + 1 < st.total_pages
                                      ? MenuAction::run_command(g_cmd_page_delta, 1)
                                      : MenuAction::none());
    next.role = MenuWidgetRole::PageNext;
    widgets.push_back(prev);
    widgets.push_back(next);

    MenuWidget code = make_text(kCodeInputWidgetId,
                                SettingsObjectID::CARD0,
                                "Room Code",
                                &st.room_code_text,
                                16);
    code.placeholder = "ABCD12";
    MenuWidget join_code = make_button(kActionWidgetId,
                                       SettingsObjectID::CARD1,
                                       "Join Room Code",
                                       MenuAction::run_command(g_cmd_join_code));
    widgets.push_back(code);
    widgets.push_back(join_code);

    std::vector<WidgetId> room_ids;
    int start = st.page * kRoomsPerPage;
    for (int i = 0; i < kRoomsPerPage; ++i) {
        int room_index = start + i;
        WidgetId widget_id = kFirstRoomWidgetId + static_cast<WidgetId>(i);
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD2 + i);
        if (room_index < static_cast<int>(ctx.engine.lobby.discovered_rooms.size())) {
            const MatchmakingRoom& room =
                ctx.engine.lobby.discovered_rooms[static_cast<std::size_t>(room_index)];
            text_cache.push_back(room.session_name.empty() ? room.room_code : room.session_name);
            std::string detail = room.host_name + " | " + std::to_string(room.current_players) +
                                 "/" + std::to_string(room.max_players);
            if (session_contract_is_in_game(room.contract))
                detail += " | In Game";
            else
                detail += " | Lobby";
            text_cache.push_back(std::move(detail));

            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            card.label = text_cache[text_cache.size() - 2].c_str();
            card.secondary = text_cache[text_cache.size() - 1].c_str();
            card.badge = room.room_code.c_str();
            card.on_select = MenuAction::run_command(g_cmd_join_listed, room_index);
            widgets.push_back(card);
            room_ids.push_back(card.id);
        } else {
            widgets.push_back(make_label(widget_id, slot, ""));
        }
    }

    MenuWidget refresh = make_button(kRefreshWidgetId,
                                     SettingsObjectID::SEARCH,
                                     "Refresh",
                                     MenuAction::run_command(g_cmd_refresh));
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(refresh);
    widgets.push_back(back);

    code.nav_down = join_code.id;
    join_code.nav_up = code.id;
    join_code.nav_down = room_ids.empty() ? refresh.id : room_ids.front();
    for (std::size_t i = 0; i < room_ids.size(); ++i) {
        for (MenuWidget& widget : widgets) {
            if (widget.id != room_ids[i])
                continue;
            widget.nav_up = (i == 0) ? join_code.id : room_ids[i - 1];
            widget.nav_down = (i + 1 < room_ids.size()) ? room_ids[i + 1] : refresh.id;
            break;
        }
    }
    refresh.nav_up = room_ids.empty() ? join_code.id : room_ids.back();
    refresh.nav_down = back.id;
    back.nav_up = refresh.id;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = code.id;
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
    if (g_cmd_host == kMenuIdInvalid)
        g_cmd_host = engine.menu_commands.register_command(command_host);
    if (g_cmd_leave == kMenuIdInvalid)
        g_cmd_leave = engine.menu_commands.register_command(command_leave);
    if (g_cmd_join_code == kMenuIdInvalid)
        g_cmd_join_code = engine.menu_commands.register_command(command_join_code);
    if (g_cmd_join_listed == kMenuIdInvalid)
        g_cmd_join_listed = engine.menu_commands.register_command(command_join_listed);
    if (g_cmd_refresh == kMenuIdInvalid)
        g_cmd_refresh = engine.menu_commands.register_command(command_refresh);
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = engine.menu_commands.register_command(command_page_delta);
    if (g_cmd_visibility_delta == kMenuIdInvalid)
        g_cmd_visibility_delta = engine.menu_commands.register_command(command_visibility_delta);
    if (g_cmd_max_players_delta == kMenuIdInvalid)
        g_cmd_max_players_delta = engine.menu_commands.register_command(command_max_players_delta);

    register_screen(engine, MenuScreenID::LOBBY_HOST_SETUP, build_host_screen);
    register_screen(engine, MenuScreenID::LOBBY_SERVER_BROWSER, build_join_screen);
}
