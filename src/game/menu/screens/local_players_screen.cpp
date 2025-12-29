#include "game/menu/screens/local_players_screen.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/player.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kPlayersPerPage = 4;
constexpr int kMaxLocalPlayers = 4;
constexpr WidgetId kTitleWidgetId = 900;
constexpr WidgetId kStatusWidgetId = 901;
constexpr WidgetId kPageLabelWidgetId = 905;
constexpr WidgetId kPrevButtonId = 903;
constexpr WidgetId kNextButtonId = 904;
constexpr WidgetId kMoreButtonId = 906;
constexpr WidgetId kLessButtonId = 907;
constexpr WidgetId kBackButtonId = 930;
constexpr WidgetId kFirstCardWidgetId = 920;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_add_player = kMenuIdInvalid;
MenuCommandId g_cmd_remove_player = kMenuIdInvalid;
MenuCommandId g_cmd_open_player = kMenuIdInvalid;

struct LocalPlayersState {
    int page = 0;
    int total_pages = 1;
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
    auto& st = ctx.state<LocalPlayersState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.total_pages <= 0) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

void command_add_player(MenuContext&, std::int32_t) {
    if (!es)
        return;
    int count = static_cast<int>(es->players.size());
    if (count >= kMaxLocalPlayers)
        return;
    add_player();
}

void command_remove_player(MenuContext&, std::int32_t) {
    if (!es)
        return;
    int count = static_cast<int>(es->players.size());
    if (count <= 1)
        return;
    remove_player(count - 1);
}

void command_open_player(MenuContext& ctx, std::int32_t index) {
    if (!es || index < 0 || index >= static_cast<int>(es->players.size()))
        return;
    lobby_state().selected_player_index = index;
    ctx.manager.push_screen(MenuScreenID::PLAYER_SETTINGS);
}

BuiltScreen build_local_players(MenuContext& ctx) {
    auto& st = ctx.state<LocalPlayersState>();
    int total_players = es ? static_cast<int>(es->players.size()) : 1;
    st.total_pages = std::max(1, (total_players + kPlayersPerPage - 1) / kPlayersPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(8);

    widgets.push_back(make_label_widget(kTitleWidgetId, LocalPlayersObjectID::TITLE, "Local Players"));

    std::string status = std::to_string(total_players) + (total_players == 1 ? " player" : " players");
    MenuWidget status_label = make_label_widget(kStatusWidgetId, LocalPlayersObjectID::STATUS, status.c_str());
    widgets.push_back(status_label);

    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, LocalPlayersObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, LocalPlayersObjectID::PREV, "<", prev_action);
    MenuWidget next_btn = make_button_widget(kNextButtonId, LocalPlayersObjectID::NEXT, ">", next_action);
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    MenuWidget more_btn = make_button_widget(kMoreButtonId,
                                             LocalPlayersObjectID::MORE,
                                             "More",
                                             MenuAction::run_command(g_cmd_add_player));
    MenuWidget less_btn = make_button_widget(kLessButtonId,
                                             LocalPlayersObjectID::LESS,
                                             "Less",
                                             MenuAction::run_command(g_cmd_remove_player));
    widgets.push_back(more_btn);
    std::size_t more_idx = widgets.size() - 1;
    widgets.push_back(less_btn);
    std::size_t less_idx = widgets.size() - 1;

    std::vector<WidgetId> card_ids;
    card_ids.reserve(kPlayersPerPage);
    std::size_t cards_offset = widgets.size();
    int start_index = st.page * kPlayersPerPage;
    for (int i = 0; i < kPlayersPerPage; ++i) {
        int player_index = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(LocalPlayersObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (es && player_index < static_cast<int>(es->players.size())) {
            const Player& player = es->players[static_cast<std::size_t>(player_index)];
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Button;
            text_cache.emplace_back("Player " + std::to_string(player_index + 1));
            card.label = text_cache.back().c_str();
            std::string profile_name = player.profile.name.empty() ? "No profile" : player.profile.name;
            text_cache.emplace_back(profile_name);
            card.badge = text_cache.back().c_str();
            card.on_select = MenuAction::run_command(g_cmd_open_player, player_index);
            card.on_left = prev_action;
            card.on_right = next_action;
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, LocalPlayersObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& more_ref = widgets[more_idx];
    MenuWidget& less_ref = widgets[less_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.front();
    WidgetId last_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.back();
    WidgetId cards_start = first_card_id != kMenuIdInvalid ? first_card_id : back_ref.id;

    more_ref.nav_right = less_ref.id;
    more_ref.nav_left = more_ref.id;
    more_ref.nav_down = cards_start;
    more_ref.nav_up = more_ref.id;

    less_ref.nav_left = more_ref.id;
    less_ref.nav_right = prev_ref.id;
    less_ref.nav_down = cards_start;
    less_ref.nav_up = less_ref.id;

    prev_ref.nav_left = less_ref.id;
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_down = cards_start;
    prev_ref.nav_up = prev_ref.id;

    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    next_ref.nav_down = cards_start;
    next_ref.nav_up = next_ref.id;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget& card = widgets[cards_offset + i];
        card.nav_left = prev_ref.id;
        card.nav_right = next_ref.id;
        card.nav_up = (i == 0) ? more_ref.id : card_ids[i - 1];
        card.nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_up = (last_card_id != kMenuIdInvalid) ? last_card_id : more_ref.id;
    back_ref.nav_left = prev_ref.id;
    back_ref.nav_right = next_ref.id;
    back_ref.nav_down = back_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::LOCAL_PLAYERS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = cards_start != kMenuIdInvalid ? cards_start : more_ref.id;
    return built;
}

} // namespace

void register_local_players_screen() {
    if (!es)
        return;

    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_add_player == kMenuIdInvalid)
        g_cmd_add_player = es->menu_commands.register_command(command_add_player);
    if (g_cmd_remove_player == kMenuIdInvalid)
        g_cmd_remove_player = es->menu_commands.register_command(command_remove_player);
    if (g_cmd_open_player == kMenuIdInvalid)
        g_cmd_open_player = es->menu_commands.register_command(command_open_player);

    MenuScreenDef def;
    def.id = MenuScreenID::LOCAL_PLAYERS;
    def.layout = UILayoutID::LOCAL_PLAYERS_SCREEN;
    def.state_ops = screen_state_ops<LocalPlayersState>();
    def.build = build_local_players;
    es->menu_manager.register_screen(def);
}
