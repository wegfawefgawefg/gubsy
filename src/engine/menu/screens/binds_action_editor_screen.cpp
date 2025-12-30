#include "engine/menu/screens/binds_action_editor_screen.hpp"

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
constexpr WidgetId kTitleWidgetId = 1700;
constexpr WidgetId kStatusWidgetId = 1701;
constexpr WidgetId kPageLabelWidgetId = 1702;
constexpr WidgetId kPrevButtonId = 1703;
constexpr WidgetId kNextButtonId = 1704;
constexpr WidgetId kBackButtonId = 1730;
constexpr WidgetId kFirstCardWidgetId = 1720;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_edit_mapping = kMenuIdInvalid;
MenuCommandId g_cmd_delete_mapping = kMenuIdInvalid;

struct MappingSlot {
    bool empty{true};
    int bind_index{-1};
    int device_code{0};
    std::string label;
};

struct BindsActionEditorState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
    std::string title_text;
    std::vector<MappingSlot> slots;
    int action_id{-1};
    BindsActionType action_type{BindsActionType::Button};
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

bool find_action_label(BindsActionType type, int action_id, std::string& label, std::string& category) {
    const BindsSchema& schema = get_binds_schema();
    if (type == BindsActionType::Button) {
        for (const auto& action : schema.actions) {
            if (action.action_id == action_id) {
                label = action.display_name;
                category = action.category;
                return true;
            }
        }
    } else if (type == BindsActionType::Analog1D) {
        for (const auto& action : schema.analogs_1d) {
            if (action.analog_id == action_id) {
                label = action.display_name;
                category = action.category;
                return true;
            }
        }
    } else if (type == BindsActionType::Analog2D) {
        for (const auto& action : schema.analogs_2d) {
            if (action.analog_id == action_id) {
                label = action.display_name;
                category = action.category;
                return true;
            }
        }
    }
    return false;
}

void rebuild_slots(BindsActionEditorState& st, const BindsProfile& profile) {
    st.slots.clear();
    if (st.action_type == BindsActionType::Button) {
        for (std::size_t i = 0; i < profile.button_binds.size(); ++i) {
            const auto& pair = profile.button_binds[i];
            if (pair.second != st.action_id)
                continue;
            MappingSlot slot;
            slot.empty = false;
            slot.bind_index = static_cast<int>(i);
            slot.device_code = pair.first;
            slot.label = binds_input_label(st.action_type, pair.first);
            st.slots.push_back(std::move(slot));
        }
    } else if (st.action_type == BindsActionType::Analog1D) {
        for (std::size_t i = 0; i < profile.analog_1d_binds.size(); ++i) {
            const auto& pair = profile.analog_1d_binds[i];
            if (pair.second != st.action_id)
                continue;
            MappingSlot slot;
            slot.empty = false;
            slot.bind_index = static_cast<int>(i);
            slot.device_code = pair.first;
            slot.label = binds_input_label(st.action_type, pair.first);
            st.slots.push_back(std::move(slot));
        }
    } else {
        for (std::size_t i = 0; i < profile.analog_2d_binds.size(); ++i) {
            const auto& pair = profile.analog_2d_binds[i];
            if (pair.second != st.action_id)
                continue;
            MappingSlot slot;
            slot.empty = false;
            slot.bind_index = static_cast<int>(i);
            slot.device_code = pair.first;
            slot.label = binds_input_label(st.action_type, pair.first);
            st.slots.push_back(std::move(slot));
        }
    }

    int mapping_count = static_cast<int>(st.slots.size());
    int total_slots = (mapping_count / kItemsPerPage + 1) * kItemsPerPage;
    for (int i = mapping_count; i < total_slots; ++i)
        st.slots.push_back(MappingSlot{});

    st.total_pages = std::max(1, total_slots / kItemsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<BindsActionEditorState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.total_pages <= 0) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

void command_edit_mapping(MenuContext& ctx, std::int32_t slot_index) {
    auto& st = ctx.state<BindsActionEditorState>();
    if (!es)
        return;
    if (slot_index < 0 || slot_index >= static_cast<int>(st.slots.size()))
        return;
    const MappingSlot& slot = st.slots[static_cast<std::size_t>(slot_index)];
    es->selected_binds_action_type = static_cast<int>(st.action_type);
    es->selected_binds_action_id = st.action_id;
    es->selected_binds_mapping_index = slot.empty ? -1 : slot.bind_index;
    ctx.manager.push_screen(MenuScreenID::BINDS_CHOOSE_INPUT, ctx.player_index);
}

void command_delete_mapping(MenuContext& ctx, std::int32_t slot_index) {
    auto& st = ctx.state<BindsActionEditorState>();
    if (!es)
        return;
    if (slot_index < 0 || slot_index >= static_cast<int>(st.slots.size()))
        return;
    const MappingSlot& slot = st.slots[static_cast<std::size_t>(slot_index)];
    if (slot.empty)
        return;

    BindsProfile* profile = find_profile(es->selected_binds_profile_id);
    if (!profile)
        return;
    if (st.action_type == BindsActionType::Button) {
        auto& binds = profile->button_binds;
        if (slot.bind_index < 0 || slot.bind_index >= static_cast<int>(binds.size()))
            return;
        binds.erase(binds.begin() + slot.bind_index);
    } else if (st.action_type == BindsActionType::Analog1D) {
        auto& binds = profile->analog_1d_binds;
        if (slot.bind_index < 0 || slot.bind_index >= static_cast<int>(binds.size()))
            return;
        binds.erase(binds.begin() + slot.bind_index);
    } else {
        auto& binds = profile->analog_2d_binds;
        if (slot.bind_index < 0 || slot.bind_index >= static_cast<int>(binds.size()))
            return;
        binds.erase(binds.begin() + slot.bind_index);
    }
    save_binds_profile(*profile);
    add_alert("Mapping removed");
}

BuiltScreen build_binds_action_editor(MenuContext& ctx) {
    auto& st = ctx.state<BindsActionEditorState>();
    if (!es) {
        BuiltScreen built;
        built.layout = UILayoutID::SETTINGS_SCREEN;
        return built;
    }

    st.action_id = es->selected_binds_action_id;
    st.action_type = static_cast<BindsActionType>(es->selected_binds_action_type);

    int profile_id = es->selected_binds_profile_id;
    BindsProfile* profile = find_profile(profile_id);
    if (!profile)
        return BuiltScreen{};

    std::string action_label;
    std::string action_category;
    bool found = find_action_label(st.action_type, st.action_id, action_label, action_category);
    if (!found) {
        action_label = "Unknown";
        action_category.clear();
    }
    st.status_text = action_label;
    if (!action_category.empty())
        st.status_text += " | " + action_category;
    st.title_text = std::string("Edit ") + binds_action_type_label(st.action_type);

    rebuild_slots(st, *profile);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label_widget(kTitleWidgetId, SettingsObjectID::TITLE, st.title_text.c_str()));
    widgets.push_back(make_label_widget(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));
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
        int slot_index = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (slot_index < static_cast<int>(st.slots.size())) {
            const MappingSlot& mapping = st.slots[static_cast<std::size_t>(slot_index)];
            MenuWidget row;
            row.id = widget_id;
            row.slot = slot;
            row.type = WidgetType::Card;
            if (mapping.empty) {
                row.label = "New Mapping";
                row.secondary = "Press to choose input.";
            } else {
                text_cache.emplace_back("Input: " + mapping.label);
                row.label = text_cache.back().c_str();
                row.secondary = "Press to change input.";
                row.tertiary = "Back: Delete";
                row.tertiary_overlay = false;
                row.on_back = MenuAction::run_command(g_cmd_delete_mapping, slot_index);
            }
            row.on_select = MenuAction::run_command(g_cmd_edit_mapping, slot_index);
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

    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_row_id = row_ids.empty() ? kMenuIdInvalid : row_ids.front();
    WidgetId last_row_id = row_ids.empty() ? kMenuIdInvalid : row_ids.back();
    WidgetId rows_start = first_row_id != kMenuIdInvalid ? first_row_id : back_ref.id;

    prev_ref.nav_left = prev_ref.id;
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_up = prev_ref.id;
    prev_ref.nav_down = rows_start;

    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    next_ref.nav_up = next_ref.id;
    next_ref.nav_down = rows_start;

    for (std::size_t i = 0; i < row_ids.size(); ++i) {
        MenuWidget& row = widgets[rows_offset + i];
        row.nav_left = prev_ref.id;
        row.nav_right = next_ref.id;
        row.nav_up = (i == 0) ? prev_ref.id : row_ids[i - 1];
        row.nav_down = (i + 1 < row_ids.size()) ? row_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_up = (last_row_id != kMenuIdInvalid) ? last_row_id : prev_ref.id;
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

void register_binds_action_editor_screen() {
    if (!es)
        return;
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_edit_mapping == kMenuIdInvalid)
        g_cmd_edit_mapping = es->menu_commands.register_command(command_edit_mapping);
    if (g_cmd_delete_mapping == kMenuIdInvalid)
        g_cmd_delete_mapping = es->menu_commands.register_command(command_delete_mapping);

    MenuScreenDef def;
    def.id = MenuScreenID::BINDS_ACTION_EDITOR;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<BindsActionEditorState>();
    def.build = build_binds_action_editor;
    es->menu_manager.register_screen(def);
}
