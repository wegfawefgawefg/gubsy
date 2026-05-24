#include "src/menu/screens/lobby_local_players_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/engine_state.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int kPlayersPerPage = 3;
constexpr WidgetId kTitleWidgetId = 2400;
constexpr WidgetId kStatusWidgetId = 2401;
constexpr WidgetId kPageLabelWidgetId = 2403;
constexpr WidgetId kPrevButtonId = 2404;
constexpr WidgetId kNextButtonId = 2405;
constexpr WidgetId kAddButtonId = 2410;
constexpr WidgetId kBackButtonId = 2430;
constexpr WidgetId kFirstCardWidgetId = 2420;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_add_player = kMenuIdInvalid;
MenuCommandId g_cmd_open_player = kMenuIdInvalid;

struct LocalPlayersState {
    int page{0};
    int total_pages{1};
    std::string page_text;
    std::string status_text;
};

MenuWidget make_label(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Label;
    widget.label = label;
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

void update_page(LocalPlayersState& st, int count) {
    st.total_pages = std::max(1, (count + kPlayersPerPage - 1) / kPlayersPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " +
                   std::to_string(st.total_pages);
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<LocalPlayersState>();
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st, static_cast<int>(ctx.engine.lobby.local_players.size()));
}

void command_add_player(MenuContext& ctx, std::int32_t) {
    int index = gubsy_lobby_add_local_player(ctx.engine);
    ctx.manager.push_screen(MenuScreenID::LOBBY_PLAYER_SETTINGS, index);
}

void command_open_player(MenuContext& ctx, std::int32_t index) {
    gubsy_lobby_select_player(ctx.engine, index);
    ctx.manager.push_screen(MenuScreenID::LOBBY_PLAYER_SETTINGS, index);
}

BuiltScreen build_local_players(MenuContext& ctx) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<LocalPlayersState>();
    int count = static_cast<int>(ctx.engine.lobby.local_players.size());
    update_page(st, count);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Local Players"));
    st.status_text = std::to_string(count) + (count == 1 ? " local player" : " local players");
    widgets.push_back(make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));
    widgets.push_back(make_label(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages)
        next_action = MenuAction::run_command(g_cmd_page_delta, 1);

    MenuWidget prev = make_button(kPrevButtonId, SettingsObjectID::PREV, "<", prev_action);
    prev.role = MenuWidgetRole::PagePrev;
    MenuWidget next = make_button(kNextButtonId, SettingsObjectID::NEXT, ">", next_action);
    next.role = MenuWidgetRole::PageNext;
    widgets.push_back(prev);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> card_ids;
    int start = st.page * kPlayersPerPage;
    for (int i = 0; i < kPlayersPerPage; ++i) {
        int player_index = start + i;
        WidgetId widget_id = kFirstCardWidgetId + static_cast<WidgetId>(i);
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        if (player_index < count) {
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            text_cache.push_back(gubsy_lobby_player_label(ctx.engine, player_index));
            const GubsyLobbyPlayer* player = gubsy_lobby_player(ctx.engine, player_index);
            std::string detail = "Devices: ";
            detail += player ? std::to_string(player->devices.size()) : "0";
            detail += "  Select to edit.";
            text_cache.push_back(std::move(detail));
            card.label = text_cache[text_cache.size() - 2].c_str();
            card.secondary = text_cache[text_cache.size() - 1].c_str();
            card.on_select = MenuAction::run_command(g_cmd_open_player, player_index);
            card.on_left = prev_action;
            card.on_right = next_action;
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label(widget_id, slot, ""));
        }
    }

    MenuWidget add = make_button(kAddButtonId,
                                 SettingsObjectID::CARD3,
                                 "Add Local Player",
                                 MenuAction::run_command(g_cmd_add_player));
    MenuWidget back = make_button(kBackButtonId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(add);
    std::size_t add_idx = widgets.size() - 1;
    widgets.push_back(back);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& add_ref = widgets[add_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_card = card_ids.empty() ? add_ref.id : card_ids.front();
    WidgetId last_card = card_ids.empty() ? add_ref.id : card_ids.back();
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_down = first_card;
    next_ref.nav_left = prev_ref.id;
    next_ref.nav_down = first_card;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget* card = nullptr;
        for (auto& widget : widgets) {
            if (widget.id == card_ids[i]) {
                card = &widget;
                break;
            }
        }
        if (!card)
            continue;
        card->nav_up = (i == 0) ? prev_ref.id : card_ids[i - 1];
        card->nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : add_ref.id;
    }
    add_ref.nav_up = last_card;
    add_ref.nav_down = back_ref.id;
    back_ref.nav_up = add_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = first_card;
    return built;
}

} // namespace

void register_lobby_local_players_screen(EngineState& engine) {
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = engine.menu_commands.register_command(command_page_delta);
    if (g_cmd_add_player == kMenuIdInvalid)
        g_cmd_add_player = engine.menu_commands.register_command(command_add_player);
    if (g_cmd_open_player == kMenuIdInvalid)
        g_cmd_open_player = engine.menu_commands.register_command(command_open_player);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY_LOCAL_PLAYERS;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<LocalPlayersState>();
    def.build = build_local_players;
    engine.menu_manager.register_screen(def);
}
