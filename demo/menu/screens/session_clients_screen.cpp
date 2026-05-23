#include "demo/menu/screens/session_clients_screen.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "engine/alerts.hpp"
#include "engine/engine_state.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "demo/menu/lobby_online.hpp"
#include "demo/menu/lobby_state.hpp"
#include "demo/menu/menu_ids.hpp"
#include "demo/ui_layout_ids.hpp"

namespace {

constexpr int kClientsPerPage = 4;
constexpr WidgetId kTitleWidgetId = 1500;
constexpr WidgetId kStatusWidgetId = 1501;
constexpr WidgetId kPageWidgetId = 1502;
constexpr WidgetId kPrevWidgetId = 1503;
constexpr WidgetId kNextWidgetId = 1504;
constexpr WidgetId kBackWidgetId = 1530;
constexpr WidgetId kFirstCardWidgetId = 1520;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_disconnect_client = kMenuIdInvalid;

struct SessionClientsState {
    int page{0};
    int total_pages{1};
    std::string page_text;
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

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<SessionClientsState>();
    const int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    st.page_text = "Page " + std::to_string(st.page + 1) +
                   " / " + std::to_string(std::max(1, st.total_pages));
}

void command_disconnect_client(MenuContext& ctx, std::int32_t member_index) {
    LobbySession& lobby = lobby_state();
    if (!lobby.online.is_host) {
        add_alert(ctx.engine, "Only the host can remove clients.");
        return;
    }
    if (member_index < 0 || member_index >= static_cast<int>(lobby.online.members.size()))
        return;
    const LobbyOnlineMember& member = lobby.online.members[static_cast<std::size_t>(member_index)];
    if (member.is_host || member.is_local) {
        add_alert(ctx.engine, "Cannot remove the local host.");
        return;
    }
    std::string err;
    if (!lobby_online_remove_member(lobby, member.member_id, err)) {
        add_alert(ctx.engine, err.empty() ? "Failed to remove client." : err);
        return;
    }
    add_alert(ctx.engine, "Removed client " + member.display_name);
}

BuiltScreen build_session_clients(MenuContext& ctx) {
    auto& st = ctx.state<SessionClientsState>();
    LobbySession& lobby = lobby_state();
    lobby_online_tick(lobby);

    const int count = static_cast<int>(lobby.online.members.size());
    st.total_pages = std::max(1, (count + kClientsPerPage - 1) / kClientsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) +
                   " / " + std::to_string(st.total_pages);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label_widget(kTitleWidgetId, SettingsObjectID::TITLE, "Connected Clients"));
    text_cache.emplace_back(lobby.online.is_host
                                ? "Host can disconnect remote members."
                                : "Viewing connected members.");
    widgets.push_back(make_label_widget(kStatusWidgetId, SettingsObjectID::STATUS, text_cache.back().c_str()));
    widgets.push_back(make_label_widget(kPageWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);
    MenuWidget prev_btn = make_button_widget(kPrevWidgetId, SettingsObjectID::PREV, "<", prev_action);
    prev_btn.role = MenuWidgetRole::PagePrev;
    MenuWidget next_btn = make_button_widget(kNextWidgetId, SettingsObjectID::NEXT, ">", next_action);
    next_btn.role = MenuWidgetRole::PageNext;
    widgets.push_back(prev_btn);
    widgets.push_back(next_btn);

    const int start_index = st.page * kClientsPerPage;
    std::vector<WidgetId> card_ids;
    for (int i = 0; i < kClientsPerPage; ++i) {
        const int member_index = start_index + i;
        const WidgetId widget_id = kFirstCardWidgetId + static_cast<WidgetId>(i);
        const UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        if (member_index < count) {
            const LobbyOnlineMember& member = lobby.online.members[static_cast<std::size_t>(member_index)];
            MenuAction select = MenuAction::none();
            const bool can_disconnect = lobby.online.is_host && !member.is_host && !member.is_local;
            if (can_disconnect)
                select = MenuAction::run_command(g_cmd_disconnect_client, member_index);

            MenuWidget card = make_button_widget(widget_id, slot, member.display_name.c_str(), select);
            text_cache.emplace_back(member.member_id);
            card.secondary = text_cache.back().c_str();
            text_cache.emplace_back(member.is_host
                                        ? "Host"
                                        : (can_disconnect ? "Press to disconnect." : "Connected client."));
            card.badge = text_cache.back().c_str();
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    widgets.push_back(make_button_widget(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop()));

    MenuWidget& prev_ref = widgets[3];
    MenuWidget& next_ref = widgets[4];
    MenuWidget& back_ref = widgets.back();
    const WidgetId first_card = card_ids.empty() ? back_ref.id : card_ids.front();
    prev_ref.nav_down = first_card;
    next_ref.nav_down = first_card;
    prev_ref.nav_right = next_ref.id;
    next_ref.nav_left = prev_ref.id;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget& card = widgets[5 + i];
        card.nav_left = prev_ref.id;
        card.nav_right = next_ref.id;
        card.nav_up = (i == 0) ? prev_ref.id : card_ids[i - 1];
        card.nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_up = card_ids.empty() ? prev_ref.id : card_ids.back();
    back_ref.nav_left = prev_ref.id;
    back_ref.nav_right = next_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = first_card;
    return built;
}

} // namespace

void register_session_clients_screen(EngineState& engine) {
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = engine.menu_commands.register_command(command_page_delta);
    if (g_cmd_disconnect_client == kMenuIdInvalid)
        g_cmd_disconnect_client = engine.menu_commands.register_command(command_disconnect_client);

    MenuScreenDef def;
    def.id = MenuScreenID::SESSION_CLIENTS;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<SessionClientsState>();
    def.build = build_session_clients;
    engine.menu_manager.register_screen(def);
}
