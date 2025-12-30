#include "engine/menu/screens/binds_choose_input_screen.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "engine/alerts.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/binds_ui_helpers.hpp"
#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kItemsPerPage = 4;
constexpr WidgetId kTitleWidgetId = 1800;
constexpr WidgetId kStatusWidgetId = 1801;
constexpr WidgetId kSearchWidgetId = 1802;
constexpr WidgetId kPageLabelWidgetId = 1803;
constexpr WidgetId kPrevButtonId = 1804;
constexpr WidgetId kNextButtonId = 1805;
constexpr WidgetId kBackButtonId = 1830;
constexpr WidgetId kFirstCardWidgetId = 1820;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_select_input = kMenuIdInvalid;

struct BindsChooseInputState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
    std::vector<int> filtered_indices;
    std::string search_query;
    std::string prev_search;
    BindsActionType action_type{BindsActionType::Button};
    std::vector<InputChoice> choices;
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

BindsProfile* find_profile(int profile_id) {
    if (!es)
        return nullptr;
    for (auto& profile : es->binds_profiles) {
        if (profile.id == profile_id)
            return &profile;
    }
    return nullptr;
}

void rebuild_filter(BindsChooseInputState& st) {
    st.filtered_indices.clear();
    std::string needle = st.search_query;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (std::size_t i = 0; i < st.choices.size(); ++i) {
        if (!needle.empty()) {
            std::string hay = st.choices[i].label ? st.choices[i].label : "";
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
    st.total_pages = std::max(1, (filtered + kItemsPerPage - 1) / kItemsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<BindsChooseInputState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.filtered_indices.empty()) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

void command_select_input(MenuContext& ctx, std::int32_t choice_index) {
    auto& st = ctx.state<BindsChooseInputState>();
    if (!es)
        return;
    if (choice_index < 0 || choice_index >= static_cast<int>(st.choices.size()))
        return;

    int profile_id = es->selected_binds_profile_id;
    BindsProfile* profile = find_profile(profile_id);
    if (!profile)
        return;

    int action_id = es->selected_binds_action_id;
    int mapping_index = es->selected_binds_mapping_index;
    int device_code = st.choices[static_cast<std::size_t>(choice_index)].code;

    if (st.action_type == BindsActionType::Button) {
        if (mapping_index >= 0 && mapping_index < static_cast<int>(profile->button_binds.size())) {
            profile->button_binds[static_cast<std::size_t>(mapping_index)] = {device_code, action_id};
        } else {
            profile->button_binds.emplace_back(device_code, action_id);
        }
    } else if (st.action_type == BindsActionType::Analog1D) {
        if (mapping_index >= 0 && mapping_index < static_cast<int>(profile->analog_1d_binds.size())) {
            profile->analog_1d_binds[static_cast<std::size_t>(mapping_index)] = {device_code, action_id};
        } else {
            profile->analog_1d_binds.emplace_back(device_code, action_id);
        }
    } else {
        if (mapping_index >= 0 && mapping_index < static_cast<int>(profile->analog_2d_binds.size())) {
            profile->analog_2d_binds[static_cast<std::size_t>(mapping_index)] = {device_code, action_id};
        } else {
            profile->analog_2d_binds.emplace_back(device_code, action_id);
        }
    }

    save_binds_profile(*profile);
    add_alert("Input bound");
    ctx.manager.pop_screen();
}

BuiltScreen build_binds_choose_input(MenuContext& ctx) {
    auto& st = ctx.state<BindsChooseInputState>();
    if (!es) {
        BuiltScreen built;
        built.layout = UILayoutID::SETTINGS_SCREEN;
        return built;
    }

    st.action_type = static_cast<BindsActionType>(es->selected_binds_action_type);
    st.choices = binds_input_choices(st.action_type);

    if (st.search_query != st.prev_search) {
        st.prev_search = st.search_query;
        rebuild_filter(st);
    } else if (st.filtered_indices.empty() || st.filtered_indices.size() != st.choices.size()) {
        rebuild_filter(st);
    }

    st.status_text = std::string("Select a ") + binds_action_type_label(st.action_type);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label_widget(kTitleWidgetId, SettingsObjectID::TITLE, "Choose Input"));
    widgets.push_back(make_label_widget(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));

    MenuWidget search;
    search.id = kSearchWidgetId;
    search.slot = SettingsObjectID::SEARCH;
    search.type = WidgetType::TextInput;
    search.text_buffer = &st.search_query;
    search.text_max_len = 48;
    search.placeholder = "Search inputs...";
    widgets.push_back(search);
    std::size_t search_idx = widgets.size() - 1;

    widgets.push_back(make_label_widget(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, SettingsObjectID::PREV, "<", prev_action);
    MenuWidget next_btn = make_button_widget(kNextButtonId, SettingsObjectID::NEXT, ">", next_action);
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> row_ids;
    row_ids.reserve(kItemsPerPage);
    std::size_t rows_offset = widgets.size();
    int start_index = st.page * kItemsPerPage;
    for (int i = 0; i < kItemsPerPage; ++i) {
        int filtered_idx = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (filtered_idx < static_cast<int>(st.filtered_indices.size())) {
            int choice_index = st.filtered_indices[static_cast<std::size_t>(filtered_idx)];
            const InputChoice& choice = st.choices[static_cast<std::size_t>(choice_index)];
            MenuWidget row;
            row.id = widget_id;
            row.slot = slot;
            row.type = WidgetType::Button;
            row.label = choice.label ? choice.label : "";
            row.secondary = "Press to bind.";
            row.on_select = MenuAction::run_command(g_cmd_select_input, choice_index);
            row.on_left = prev_action;
            row.on_right = next_action;
            widgets.push_back(row);
            row_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& search_ref = widgets[search_idx];
    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_row_id = row_ids.empty() ? kMenuIdInvalid : row_ids.front();
    WidgetId last_row_id = row_ids.empty() ? kMenuIdInvalid : row_ids.back();
    WidgetId rows_start = first_row_id != kMenuIdInvalid ? first_row_id : back_ref.id;

    search_ref.nav_down = rows_start;
    search_ref.nav_left = search_ref.id;
    search_ref.nav_right = prev_ref.id;
    search_ref.nav_up = search_ref.id;

    prev_ref.nav_left = search_ref.id;
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_up = search_ref.id;
    prev_ref.nav_down = rows_start;

    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    next_ref.nav_up = search_ref.id;
    next_ref.nav_down = rows_start;

    for (std::size_t i = 0; i < row_ids.size(); ++i) {
        MenuWidget& row = widgets[rows_offset + i];
        row.nav_left = prev_ref.id;
        row.nav_right = next_ref.id;
        row.nav_up = (i == 0) ? search_ref.id : row_ids[i - 1];
        row.nav_down = (i + 1 < row_ids.size()) ? row_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_up = (last_row_id != kMenuIdInvalid) ? last_row_id : search_ref.id;
    back_ref.nav_left = prev_ref.id;
    back_ref.nav_right = next_ref.id;
    back_ref.nav_down = back_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = rows_start;
    return built;
}

} // namespace

void register_binds_choose_input_screen() {
    if (!es)
        return;
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_select_input == kMenuIdInvalid)
        g_cmd_select_input = es->menu_commands.register_command(command_select_input);

    MenuScreenDef def;
    def.id = MenuScreenID::BINDS_CHOOSE_INPUT;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<BindsChooseInputState>();
    def.build = build_binds_choose_input;
    es->menu_manager.register_screen(def);
}
