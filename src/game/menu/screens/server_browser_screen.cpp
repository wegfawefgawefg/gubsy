#include "game/menu/screens/server_browser_screen.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "engine/alerts.hpp"
#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "game/menu/lobby_online.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kListedRoomsPerPage = 3;
constexpr WidgetId kTitleWidgetId = 700;
constexpr WidgetId kStatusWidgetId = 701;
constexpr WidgetId kFirstCardWidgetId = 720;
constexpr WidgetId kBackButtonId = 730;

MenuCommandId g_cmd_host_or_leave = kMenuIdInvalid;
MenuCommandId g_cmd_join_room = kMenuIdInvalid;

struct ServerBrowserState {
    std::string status_text;
};

MenuWidget make_label_widget(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Label;
    w.label = label;
    return w;
}

MenuWidget make_button_widget(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Button;
    w.label = label;
    w.on_select = action;
    return w;
}

void command_host_or_leave(MenuContext&, std::int32_t) {
    LobbySession& lobby = lobby_state();
    std::string err;
    if (lobby.online.in_room) {
        if (!lobby_online_leave_room(lobby, err) && !err.empty())
            add_alert(err);
        else
            add_alert("Left online room.");
        return;
    }
    if (!lobby_online_host_current_room(lobby, err)) {
        add_alert(err.empty() ? "Failed to host room." : err);
        return;
    }
    add_alert("Hosted online room: " + lobby.online.room_code);
}

void command_join_room(MenuContext&, std::int32_t index) {
    LobbySession& lobby = lobby_state();
    if (index < 0 || index >= static_cast<int>(lobby.online.discovered_rooms.size()))
        return;
    if (lobby.online.in_room) {
        add_alert("Leave the current room before joining another.");
        return;
    }
    std::string err;
    const auto& room = lobby.online.discovered_rooms[static_cast<std::size_t>(index)];
    if (!lobby_online_join_room(lobby, room.room_code, err)) {
        add_alert(err.empty() ? "Failed to join room." : err);
        return;
    }
    add_alert("Joined room " + room.room_code);
}

BuiltScreen build_server_browser(MenuContext& ctx) {
    auto& st = ctx.state<ServerBrowserState>();
    LobbySession& lobby = lobby_state();
    lobby_online_tick(lobby);

    std::string err;
    if (!lobby_online_refresh_rooms(lobby, false, err) && !err.empty())
        st.status_text = err;
    else if (lobby.online.in_room && !lobby.online.status_text.empty())
        st.status_text = lobby.online.status_text;
    else
        st.status_text = std::to_string(lobby.online.discovered_rooms.size()) + " rooms visible";

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(16);

    widgets.push_back(make_label_widget(kTitleWidgetId, ServerBrowserObjectID::TITLE, "Server Browser"));
    widgets.push_back(make_label_widget(kStatusWidgetId,
                                        ServerBrowserObjectID::STATUS,
                                        st.status_text.c_str()));

    text_cache.emplace_back(lobby.online.in_room ? "Leave Online Room" : "Host Current Lobby Online");
    MenuWidget host_btn = make_button_widget(kFirstCardWidgetId,
                                             ServerBrowserObjectID::CARD0,
                                             text_cache.back().c_str(),
                                             MenuAction::run_command(g_cmd_host_or_leave));
    host_btn.secondary = lobby.online.server_url.c_str();
    if (lobby.online.in_room && !lobby.online.room_code.empty())
        host_btn.badge = lobby.online.room_code.c_str();
    widgets.push_back(host_btn);

    std::vector<WidgetId> selectable_ids;
    selectable_ids.push_back(host_btn.id);

    for (int i = 0; i < kListedRoomsPerPage; ++i) {
        int room_index = i;
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + 1 + static_cast<WidgetId>(i));
        UILayoutObjectId slot =
            static_cast<UILayoutObjectId>(ServerBrowserObjectID::CARD1 + static_cast<UILayoutObjectId>(i));
        if (room_index < static_cast<int>(lobby.online.discovered_rooms.size())) {
            const auto& room = lobby.online.discovered_rooms[static_cast<std::size_t>(room_index)];
            text_cache.emplace_back(room.session_name.empty() ? room.room_code : room.session_name);
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Button;
            card.label = text_cache.back().c_str();
            text_cache.emplace_back((room.session_phase == "in_game" ? std::string("In Game") : std::string("Lobby")) +
                                    " | " + room.host_name + " | " +
                                    std::to_string(room.current_players) + "/" +
                                    std::to_string(room.max_players));
            card.secondary = text_cache.back().c_str();
            text_cache.emplace_back(room.room_code + " | " + room.mod_hash.substr(0, std::min<std::size_t>(8, room.mod_hash.size())));
            card.badge = text_cache.back().c_str();
            card.on_select = MenuAction::run_command(g_cmd_join_room, room_index);
            widgets.push_back(card);
            selectable_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, ServerBrowserObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);

    MenuWidget& host_ref = widgets[2];
    MenuWidget& back_ref = widgets.back();
    host_ref.nav_up = host_ref.id;
    host_ref.nav_left = host_ref.id;
    host_ref.nav_right = host_ref.id;
    host_ref.nav_down = selectable_ids.size() > 1 ? selectable_ids[1] : back_ref.id;

    for (std::size_t i = 1; i < selectable_ids.size(); ++i) {
        MenuWidget& card = widgets[2 + i];
        card.nav_up = (i == 1) ? host_ref.id : selectable_ids[i - 1];
        card.nav_down = (i + 1 < selectable_ids.size()) ? selectable_ids[i + 1] : back_ref.id;
        card.nav_left = host_ref.id;
        card.nav_right = host_ref.id;
    }

    back_ref.nav_up = selectable_ids.empty() ? host_ref.id : selectable_ids.back();
    back_ref.nav_left = host_ref.id;
    back_ref.nav_right = host_ref.id;
    back_ref.nav_down = back_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::SERVER_BROWSER_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = host_ref.id;
    return built;
}

} // namespace

void register_server_browser_screen() {
    if (!es)
        return;

    if (g_cmd_host_or_leave == kMenuIdInvalid)
        g_cmd_host_or_leave = es->menu_commands.register_command(command_host_or_leave);
    if (g_cmd_join_room == kMenuIdInvalid)
        g_cmd_join_room = es->menu_commands.register_command(command_join_room);

    MenuScreenDef def;
    def.id = MenuScreenID::SERVER_BROWSER;
    def.layout = UILayoutID::SERVER_BROWSER_SCREEN;
    def.state_ops = screen_state_ops<ServerBrowserState>();
    def.build = build_server_browser;
    es->menu_manager.register_screen(def);
}
