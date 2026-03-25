#include "engine/menu/screens/binds_profile_editor_screen.hpp"

#include <algorithm>
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
constexpr WidgetId kTitleWidgetId = 1600;
constexpr WidgetId kStatusWidgetId = 1601;
constexpr WidgetId kSearchWidgetId = 1602;
constexpr WidgetId kPageLabelWidgetId = 1603;
constexpr WidgetId kPrevButtonId = 1604;
constexpr WidgetId kNextButtonId = 1605;
constexpr WidgetId kBackButtonId = 1630;
constexpr WidgetId kFirstCardWidgetId = 1620;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_open_action = kMenuIdInvalid;
MenuCommandId g_cmd_reset_profile = kMenuIdInvalid;

enum class EntryKind {
    Name,
    Binding,
    Reset,
};

struct EditorEntry {
    EntryKind kind{EntryKind::Binding};
    int id{0};
    BindsActionType action_type{BindsActionType::Button};
    std::string label;
    std::string category;
};

struct BindsProfileEditorState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
    std::vector<int> filtered_indices;
    std::string search_query;
    std::string prev_search;
    std::vector<EditorEntry> entries;
    std::string name_buffer;
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

MenuStyle yellow_style() {
    MenuStyle style;
    style.bg_r = 88;
    style.bg_g = 70;
    style.bg_b = 28;
    style.focus_r = 255;
    style.focus_g = 210;
    style.focus_b = 120;
    return style;
}

MenuStyle green_style() {
    MenuStyle style;
    style.bg_r = 30;
    style.bg_g = 60;
    style.bg_b = 42;
    style.focus_r = 120;
    style.focus_g = 230;
    style.focus_b = 170;
    return style;
}

MenuStyle red_style() {
    MenuStyle style;
    style.bg_r = 70;
    style.bg_g = 18;
    style.bg_b = 18;
    style.fg_r = 230;
    style.fg_g = 210;
    style.fg_b = 210;
    style.focus_r = 200;
    style.focus_g = 90;
    style.focus_b = 90;
    return style;
}

MenuStyle name_style() {
    MenuStyle style;
    style.bg_r = 28;
    style.bg_g = 36;
    style.bg_b = 68;
    style.focus_r = 140;
    style.focus_g = 200;
    style.focus_b = 255;
    return style;
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

void rebuild_entries(BindsProfileEditorState& st) {
    st.entries.clear();
    EditorEntry name_entry;
    name_entry.kind = EntryKind::Name;
    name_entry.label = "Profile Name";
    st.entries.push_back(name_entry);

    EditorEntry reset_entry;
    reset_entry.kind = EntryKind::Reset;
    reset_entry.label = "Reset to Defaults";
    st.entries.push_back(std::move(reset_entry));

    const BindsSchema& schema = get_binds_schema();
    for (const auto& action : schema.actions) {
        EditorEntry entry;
        entry.kind = EntryKind::Binding;
        entry.id = action.action_id;
        entry.action_type = BindsActionType::Button;
        entry.label = action.display_name;
        entry.category = action.category;
        st.entries.push_back(std::move(entry));
    }
    for (const auto& analog : schema.analogs_1d) {
        EditorEntry entry;
        entry.kind = EntryKind::Binding;
        entry.id = analog.analog_id;
        entry.action_type = BindsActionType::Analog1D;
        entry.label = analog.display_name;
        entry.category = analog.category;
        st.entries.push_back(std::move(entry));
    }
    for (const auto& analog : schema.analogs_2d) {
        EditorEntry entry;
        entry.kind = EntryKind::Binding;
        entry.id = analog.analog_id;
        entry.action_type = BindsActionType::Analog2D;
        entry.label = analog.display_name;
        entry.category = analog.category;
        st.entries.push_back(std::move(entry));
    }
}

void rebuild_filter(BindsProfileEditorState& st) {
    st.filtered_indices.clear();
    std::string needle = st.search_query;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (std::size_t i = 0; i < st.entries.size(); ++i) {
        if (!needle.empty()) {
            std::string hay = st.entries[i].label + " " + st.entries[i].category;
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
    auto& st = ctx.state<BindsProfileEditorState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.filtered_indices.empty()) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

void command_open_action(MenuContext& ctx, std::int32_t entry_index) {
    auto& st = ctx.state<BindsProfileEditorState>();
    if (!es)
        return;
    if (entry_index < 0 || entry_index >= static_cast<int>(st.entries.size()))
        return;
    const EditorEntry& entry = st.entries[static_cast<std::size_t>(entry_index)];
    if (entry.kind != EntryKind::Binding)
        return;
    es->selected_binds_action_type = static_cast<int>(entry.action_type);
    es->selected_binds_action_id = entry.id;
    es->selected_binds_mapping_index = -1;
    ctx.manager.push_screen(MenuScreenID::BINDS_ACTION_EDITOR, ctx.player_index);
}

void command_reset_profile(MenuContext& ctx, std::int32_t) {
    if (!es)
        return;
    int profile_id = es->selected_binds_profile_id;
    BindsProfile* profile = find_profile(profile_id);
    if (!profile)
        return;
    profile->button_binds.clear();
    profile->analog_1d_binds.clear();
    profile->analog_2d_binds.clear();
    save_binds_profile(*profile);
    ctx.state<BindsProfileEditorState>().status_text = "Binds reset to defaults";
    add_alert("Binds reset to defaults");
}

BuiltScreen build_binds_profile_editor(MenuContext& ctx) {
    auto& st = ctx.state<BindsProfileEditorState>();
    if (!es) {
        BuiltScreen built;
        built.layout = UILayoutID::SETTINGS_SCREEN;
        return built;
    }

    int profile_id = es->selected_binds_profile_id;
    BindsProfile* profile = find_profile(profile_id);
    if (!profile)
        return BuiltScreen{};

    if (st.entries.empty())
        rebuild_entries(st);

    if (st.name_buffer.empty())
        st.name_buffer = profile->name;

    if (st.search_query != st.prev_search) {
        st.prev_search = st.search_query;
        rebuild_filter(st);
    } else if (st.filtered_indices.empty() || st.filtered_indices.size() != st.entries.size()) {
        rebuild_filter(st);
    }

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label_widget(kTitleWidgetId, SettingsObjectID::TITLE, "Binds Profile"));

    st.status_text = profile->name.empty() ? "Unnamed profile" : profile->name;
    MenuWidget status_label = make_label_widget(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str());
    widgets.push_back(status_label);

    MenuWidget search;
    search.id = kSearchWidgetId;
    search.slot = SettingsObjectID::SEARCH;
    search.type = WidgetType::TextInput;
    search.text_buffer = &st.search_query;
    search.text_max_len = 48;
    search.placeholder = "Search actions...";
    widgets.push_back(search);
    std::size_t search_idx = widgets.size() - 1;

    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str());
    widgets.push_back(page_label);

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
            int entry_index = st.filtered_indices[static_cast<std::size_t>(filtered_idx)];
            const EditorEntry& entry = st.entries[static_cast<std::size_t>(entry_index)];
            MenuWidget row;
            row.id = widget_id;
            row.slot = slot;
            row.type = WidgetType::Card;
            row.label = entry.label.c_str();
            if (entry.kind == EntryKind::Name) {
                row.type = WidgetType::TextInput;
                row.label = "Profile Name";
                row.secondary = "Select to rename this profile.";
                row.text_buffer = &st.name_buffer;
                row.text_max_len = 32;
                row.on_select = MenuAction::none();
                row.style = name_style();
            } else if (entry.kind == EntryKind::Binding) {
                row.on_select = MenuAction::run_command(g_cmd_open_action, entry_index);
                std::string binds_summary;
                if (profile) {
                    auto append_label = [&](int code) {
                        if (!binds_summary.empty())
                            binds_summary += ", ";
                        binds_summary += binds_input_label(entry.action_type, code);
                    };
                    if (entry.action_type == BindsActionType::Button) {
                        for (const auto& bind : profile->button_binds) {
                            if (bind.second == entry.id)
                                append_label(bind.first);
                        }
                    } else if (entry.action_type == BindsActionType::Analog1D) {
                        for (const auto& bind : profile->analog_1d_binds) {
                            if (bind.second == entry.id)
                                append_label(bind.first);
                        }
                    } else {
                        for (const auto& bind : profile->analog_2d_binds) {
                            if (bind.second == entry.id)
                                append_label(bind.first);
                        }
                    }
                }
                if (!binds_summary.empty()) {
                    text_cache.push_back(binds_summary);
                    row.secondary = text_cache.back().c_str();
                    row.style = green_style();
                } else {
                    row.secondary = "Unbound";
                    row.style = red_style();
                }
            } else if (entry.kind == EntryKind::Reset) {
                row.on_select = MenuAction::run_command(g_cmd_reset_profile);
                row.secondary = "Clear all bindings for this profile.";
                row.style = yellow_style();
            }
            row.on_left = prev_action;
            row.on_right = next_action;
            widgets.push_back(row);
            row_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    if (profile->name != st.name_buffer && !st.name_buffer.empty()) {
        profile->name = st.name_buffer;
        save_binds_profile(*profile);
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
    built.default_focus = !row_ids.empty() ? row_ids.front() : back_ref.id;
    return built;
}

} // namespace

void register_binds_profile_editor_screen() {
    if (!es)
        return;
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_open_action == kMenuIdInvalid)
        g_cmd_open_action = es->menu_commands.register_command(command_open_action);
    if (g_cmd_reset_profile == kMenuIdInvalid)
        g_cmd_reset_profile = es->menu_commands.register_command(command_reset_profile);

    MenuScreenDef def;
    def.id = MenuScreenID::BINDS_PROFILE_EDITOR;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<BindsProfileEditorState>();
    def.build = build_binds_profile_editor;
    es->menu_manager.register_screen(def);
}
