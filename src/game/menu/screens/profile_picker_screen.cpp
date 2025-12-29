#include "game/menu/screens/profile_picker_screen.hpp"

#include <algorithm>
#include <cctype>
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

constexpr int kProfilesPerPage = 4;
constexpr WidgetId kTitleWidgetId = 1100;
constexpr WidgetId kStatusWidgetId = 1101;
constexpr WidgetId kSearchWidgetId = 1102;
constexpr WidgetId kPageLabelWidgetId = 1103;
constexpr WidgetId kPrevButtonId = 1104;
constexpr WidgetId kNextButtonId = 1105;
constexpr WidgetId kManageButtonId = 1124;
constexpr WidgetId kBackButtonId = 1130;
constexpr WidgetId kFirstCardWidgetId = 1120;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_select_profile = kMenuIdInvalid;
MenuCommandId g_cmd_open_profiles = kMenuIdInvalid;

struct ProfilePickerState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
    std::vector<int> filtered_indices;
    std::string search_query;
    std::string prev_search;
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
    auto& st = ctx.state<ProfilePickerState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.filtered_indices.empty()) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

void command_select_profile(MenuContext&, std::int32_t index) {
    if (!es)
        return;
    auto& pool = es->user_profiles_pool;
    if (index < 0 || index >= static_cast<int>(pool.size()))
        return;
    int player_index = lobby_state().selected_player_index;
    set_user_profile_for_player(player_index, pool[static_cast<std::size_t>(index)].id);
}

void command_open_profiles(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::SETTINGS);
}

void rebuild_filter(ProfilePickerState& st, const std::vector<UserProfile>& profiles) {
    st.filtered_indices.clear();
    std::string needle = st.search_query;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (!needle.empty()) {
            std::string hay = profiles[i].name;
            std::transform(hay.begin(), hay.end(), hay.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (hay.find(needle) == std::string::npos)
                continue;
        }
        st.filtered_indices.push_back(static_cast<int>(i));
    }

    int filtered = static_cast<int>(st.filtered_indices.size());
    if (filtered == 0) {
        st.page = 0;
        st.total_pages = 1;
        st.page_text = "Page 0 / 0";
        return;
    }
    st.total_pages = std::max(1, (filtered + kProfilesPerPage - 1) / kProfilesPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

BuiltScreen build_profile_picker(MenuContext& ctx) {
    auto& st = ctx.state<ProfilePickerState>();
    if (!es) {
        BuiltScreen built;
        built.layout = UILayoutID::PROFILE_PICKER_SCREEN;
        return built;
    }

    auto& profiles = es->user_profiles_pool;
    if (st.search_query != st.prev_search) {
        st.prev_search = st.search_query;
        rebuild_filter(st, profiles);
    } else if (st.filtered_indices.empty() || st.filtered_indices.size() != profiles.size()) {
        rebuild_filter(st, profiles);
    }

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label_widget(kTitleWidgetId, ProfilePickerObjectID::TITLE, "Select Profile"));

    st.status_text = std::to_string(st.filtered_indices.size()) +
                     (st.filtered_indices.size() == 1 ? " profile" : " profiles");
    MenuWidget status_label = make_label_widget(kStatusWidgetId, ProfilePickerObjectID::STATUS, st.status_text.c_str());
    widgets.push_back(status_label);

    MenuWidget search;
    search.id = kSearchWidgetId;
    search.slot = ProfilePickerObjectID::SEARCH;
    search.type = WidgetType::TextInput;
    search.text_buffer = &st.search_query;
    search.text_max_len = 48;
    search.placeholder = "Search profiles...";
    widgets.push_back(search);
    std::size_t search_idx = widgets.size() - 1;

    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, ProfilePickerObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, ProfilePickerObjectID::PREV, "<", prev_action);
    MenuWidget next_btn = make_button_widget(kNextButtonId, ProfilePickerObjectID::NEXT, ">", next_action);
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> card_ids;
    card_ids.reserve(kProfilesPerPage);
    std::size_t cards_offset = widgets.size();
    int start_index = st.page * kProfilesPerPage;
    for (int i = 0; i < kProfilesPerPage; ++i) {
        int filtered_idx = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(ProfilePickerObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (filtered_idx < static_cast<int>(st.filtered_indices.size())) {
            int profile_index = st.filtered_indices[static_cast<std::size_t>(filtered_idx)];
            const UserProfile& profile = profiles[static_cast<std::size_t>(profile_index)];
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Button;
            card.label = profile.name.c_str();
            card.on_select = MenuAction::run_command(g_cmd_select_profile, profile_index);
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget manage_btn = make_button_widget(kManageButtonId,
                                               ProfilePickerObjectID::MANAGE,
                                               "Manage Profiles...",
                                               MenuAction::run_command(g_cmd_open_profiles));
    widgets.push_back(manage_btn);
    std::size_t manage_idx = widgets.size() - 1;

    MenuWidget back_btn = make_button_widget(kBackButtonId, ProfilePickerObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& search_ref = widgets[search_idx];
    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& manage_ref = widgets[manage_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.front();
    WidgetId last_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.back();
    WidgetId cards_start = first_card_id != kMenuIdInvalid ? first_card_id : manage_ref.id;

    search_ref.nav_down = cards_start;
    search_ref.nav_left = search_ref.id;
    search_ref.nav_right = prev_ref.id;
    search_ref.nav_up = search_ref.id;

    prev_ref.nav_left = search_ref.id;
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_up = search_ref.id;
    prev_ref.nav_down = cards_start;

    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    next_ref.nav_up = search_ref.id;
    next_ref.nav_down = cards_start;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget& card = widgets[cards_offset + i];
        card.nav_left = prev_ref.id;
        card.nav_right = next_ref.id;
        card.nav_up = (i == 0) ? search_ref.id : card_ids[i - 1];
        card.nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : manage_ref.id;
    }

    manage_ref.nav_up = (last_card_id != kMenuIdInvalid) ? last_card_id : search_ref.id;
    manage_ref.nav_down = back_ref.id;
    manage_ref.nav_left = prev_ref.id;
    manage_ref.nav_right = next_ref.id;

    back_ref.nav_up = manage_ref.id;
    back_ref.nav_left = prev_ref.id;
    back_ref.nav_right = next_ref.id;
    back_ref.nav_down = back_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::PROFILE_PICKER_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = !card_ids.empty() ? card_ids.front() : manage_ref.id;
    return built;
}

} // namespace

void register_profile_picker_screen() {
    if (!es)
        return;

    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_select_profile == kMenuIdInvalid)
        g_cmd_select_profile = es->menu_commands.register_command(command_select_profile);
    if (g_cmd_open_profiles == kMenuIdInvalid)
        g_cmd_open_profiles = es->menu_commands.register_command(command_open_profiles);

    MenuScreenDef def;
    def.id = MenuScreenID::PROFILE_PICKER;
    def.layout = UILayoutID::PROFILE_PICKER_SCREEN;
    def.state_ops = screen_state_ops<ProfilePickerState>();
    def.build = build_profile_picker;
    es->menu_manager.register_screen(def);
}
