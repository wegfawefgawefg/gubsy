#include "demo/menu/screens/in_game_session_screen.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "src/alerts.hpp"
#include "src/engine_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "demo/coop_session.hpp"
#include "demo/in_game_menu.hpp"
#include "demo/menu/lobby_online.hpp"
#include "demo/menu/lobby_state.hpp"
#include "demo/menu/menu_ids.hpp"
#include "demo/modes.hpp"
#include "demo/ui_layout_ids.hpp"

namespace {

constexpr int kItemsPerPage = 4;
constexpr WidgetId kTitleWidgetId = 1400;
constexpr WidgetId kStatusWidgetId = 1401;
constexpr WidgetId kPageWidgetId = 1402;
constexpr WidgetId kPrevWidgetId = 1403;
constexpr WidgetId kNextWidgetId = 1404;
constexpr WidgetId kBackWidgetId = 1430;
constexpr WidgetId kFirstCardWidgetId = 1420;

enum class SessionAction {
    Resume,
    SaveGame,
    LoadGame,
    SessionSettings,
    ManageMods,
    ManageClients,
    Settings,
    LeaveSession,
    QuitGame,
};

struct SessionMenuItem {
    const char* label;
    const char* secondary;
    SessionAction action;
};

constexpr std::array<SessionMenuItem, 9> kSessionMenuItems{{
    {"Resume Play", "Close the session menu.", SessionAction::Resume},
    {"Session Settings", "Change host-controlled game settings.", SessionAction::SessionSettings},
    {"Manage Mods", "Change active gameplay mods.", SessionAction::ManageMods},
    {"Manage Clients", "Inspect connected players.", SessionAction::ManageClients},
    {"Save Game", "Not implemented yet.", SessionAction::SaveGame},
    {"Load Game", "Not implemented yet.", SessionAction::LoadGame},
    {"Settings", "Open engine and player settings.", SessionAction::Settings},
    {"Leave Session", "Return to the session lobby.", SessionAction::LeaveSession},
    {"Quit Game", "Exit the game process.", SessionAction::QuitGame},
}};

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_activate_item = kMenuIdInvalid;
MenuCommandId g_cmd_close_menu = kMenuIdInvalid;

struct InGameSessionMenuState {
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
    auto& st = ctx.state<InGameSessionMenuState>();
    const int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    st.page_text = "Page " + std::to_string(st.page + 1) +
                   " / " + std::to_string(std::max(1, st.total_pages));
}

void close_menu(EngineState& engine) {
    in_game_menu_reset(engine);
}

void leave_session_to_lobby(EngineState& engine) {
    LobbySession& lobby = lobby_state();
    coop_session_reset();
    if (!lobby.online.in_room || lobby.online.is_host) {
        lobby.online.contract.session_phase = "lobby";
        lobby.online.contract.realtime_endpoint.clear();
    } else {
        std::string err;
        lobby_online_leave_room(lobby, err);
        if (!err.empty())
            add_alert(engine, err);
    }
    engine.mode = modes::TITLE;
}

void command_close_menu(MenuContext& ctx, std::int32_t) {
    close_menu(ctx.engine);
}

void command_activate_item(MenuContext& ctx, std::int32_t item_index) {
    if (item_index < 0 || item_index >= static_cast<int>(kSessionMenuItems.size()))
        return;
    const SessionAction action = kSessionMenuItems[static_cast<std::size_t>(item_index)].action;
    switch (action) {
        case SessionAction::Resume:
            close_menu(ctx.engine);
            return;
        case SessionAction::SaveGame:
            add_alert(ctx.engine, "Save Game is not implemented yet.");
            return;
        case SessionAction::LoadGame:
            add_alert(ctx.engine, "Load Game is not implemented yet.");
            return;
        case SessionAction::SessionSettings:
            ctx.manager.push_screen(MenuScreenID::GAME_SETTINGS);
            return;
        case SessionAction::ManageMods:
            if (lobby_state_const().online.in_room && !lobby_state_const().online.is_host) {
                add_alert(ctx.engine, "Only the host can change online mods.");
                return;
            }
            ctx.manager.push_screen(MenuScreenID::LOBBY_MODS);
            return;
        case SessionAction::ManageClients:
            ctx.manager.push_screen(MenuScreenID::SESSION_CLIENTS);
            return;
        case SessionAction::Settings:
            ctx.manager.push_screen(MenuScreenID::SETTINGS);
            return;
        case SessionAction::LeaveSession:
            leave_session_to_lobby(ctx.engine);
            close_menu(ctx.engine);
            return;
        case SessionAction::QuitGame:
            ctx.engine.running = false;
            return;
    }
}

BuiltScreen build_in_game_session_menu(MenuContext& ctx) {
    auto& st = ctx.state<InGameSessionMenuState>();
    st.total_pages = std::max(1, (static_cast<int>(kSessionMenuItems.size()) + kItemsPerPage - 1) / kItemsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) +
                   " / " + std::to_string(st.total_pages);

    static std::vector<MenuWidget> widgets;
    widgets.clear();

    const LobbySession& lobby = lobby_state_const();
    widgets.push_back(make_label_widget(kTitleWidgetId, SettingsObjectID::TITLE, "Session Menu"));
    widgets.push_back(make_label_widget(kStatusWidgetId,
                                        SettingsObjectID::STATUS,
                                        lobby.online.status_text.empty()
                                            ? "In-game session controls."
                                            : lobby.online.status_text.c_str()));
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

    const int start_index = st.page * kItemsPerPage;
    std::vector<WidgetId> card_ids;
    for (int i = 0; i < kItemsPerPage; ++i) {
        const int item_index = start_index + i;
        const WidgetId widget_id = kFirstCardWidgetId + static_cast<WidgetId>(i);
        const UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        if (item_index < static_cast<int>(kSessionMenuItems.size())) {
            const SessionMenuItem& item = kSessionMenuItems[static_cast<std::size_t>(item_index)];
            MenuWidget card = make_button_widget(widget_id,
                                                 slot,
                                                 item.label,
                                                 MenuAction::run_command(g_cmd_activate_item, item_index));
            card.secondary = item.secondary;
            card.on_back = MenuAction::run_command(g_cmd_close_menu);
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back = make_button_widget(kBackWidgetId,
                                         SettingsObjectID::BACK,
                                         "Resume",
                                         MenuAction::run_command(g_cmd_close_menu));
    back.on_back = MenuAction::run_command(g_cmd_close_menu);
    widgets.push_back(back);

    MenuWidget& prev_ref = widgets[3];
    MenuWidget& next_ref = widgets[4];
    MenuWidget& back_ref = widgets.back();
    const WidgetId first_card = card_ids.empty() ? back_ref.id : card_ids.front();
    prev_ref.nav_down = first_card;
    next_ref.nav_down = first_card;
    prev_ref.nav_right = next_ref.id;
    next_ref.nav_left = prev_ref.id;
    prev_ref.on_back = MenuAction::run_command(g_cmd_close_menu);
    next_ref.on_back = MenuAction::run_command(g_cmd_close_menu);

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

void register_in_game_session_screen(EngineState& engine) {
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = engine.menu_commands.register_command(command_page_delta);
    if (g_cmd_activate_item == kMenuIdInvalid)
        g_cmd_activate_item = engine.menu_commands.register_command(command_activate_item);
    if (g_cmd_close_menu == kMenuIdInvalid)
        g_cmd_close_menu = engine.menu_commands.register_command(command_close_menu);

    MenuScreenDef def;
    def.id = MenuScreenID::IN_GAME_SESSION;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<InGameSessionMenuState>();
    def.build = build_in_game_session_menu;
    engine.menu_manager.register_screen(def);
}
