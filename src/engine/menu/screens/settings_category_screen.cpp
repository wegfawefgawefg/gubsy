#include "engine/menu/settings_category_registry.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/settings_catalog.hpp"
#include "game/state.hpp"
#include "engine/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr std::size_t kMaxCategoryScreens = 48;
constexpr int kSettingsPerPage = 4;

MenuCommandId g_cmd_toggle_setting = kMenuIdInvalid;
MenuCommandId g_cmd_slider_inc = kMenuIdInvalid;
MenuCommandId g_cmd_slider_dec = kMenuIdInvalid;
MenuCommandId g_cmd_option_prev = kMenuIdInvalid;
MenuCommandId g_cmd_option_next = kMenuIdInvalid;
MenuCommandId g_cmd_page_delta = kMenuIdInvalid;

std::unordered_map<std::string, MenuScreenId> g_tag_to_screen;
std::unordered_map<MenuScreenId, std::string> g_screen_to_tag;

struct EntryBinding {
    SettingsCatalogEntry entry;
};

struct SettingsCategoryState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
    std::vector<EntryBinding> entries;
    GameSettings* profile_settings = nullptr;
    std::string tag;
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

EntryBinding* get_entry_binding(MenuContext& ctx, std::int32_t index) {
    auto& st = ctx.state<SettingsCategoryState>();
    if (index < 0 || index >= static_cast<int>(st.entries.size()))
        return nullptr;
    return &st.entries[static_cast<std::size_t>(index)];
}

void persist_binding(const EntryBinding& binding, GameSettings* profile_settings) {
    if (binding.entry.install_scope) {
        save_top_level_game_settings(es->top_level_game_settings);
    } else if (profile_settings) {
        save_game_settings(*profile_settings);
    }
}

void command_toggle_setting(MenuContext& ctx, std::int32_t index) {
    EntryBinding* binding = get_entry_binding(ctx, index);
    if (!binding || !binding->entry.value)
        return;
    if (int* iv = std::get_if<int>(binding->entry.value)) {
        *iv = (*iv == 0) ? 1 : 0;
    } else if (float* fv = std::get_if<float>(binding->entry.value)) {
        *fv = (*fv >= 0.5f) ? 0.0f : 1.0f;
    }
    persist_binding(*binding, ctx.state<SettingsCategoryState>().profile_settings);
}

void adjust_slider(MenuContext& ctx, std::int32_t index, float direction) {
    EntryBinding* binding = get_entry_binding(ctx, index);
    if (!binding || !binding->entry.value || !binding->entry.metadata)
        return;
    const SettingWidgetDesc& desc = binding->entry.metadata->widget;
    float min = desc.min;
    float max = desc.max;
    float step = desc.step > 0.0f ? desc.step : (max - min) * 0.05f;
    if (float* fv = std::get_if<float>(binding->entry.value)) {
        *fv = std::clamp(*fv + step * direction, min, max);
        persist_binding(*binding, ctx.state<SettingsCategoryState>().profile_settings);
    }
}

void command_slider_inc(MenuContext& ctx, std::int32_t index) {
    adjust_slider(ctx, index, +1.0f);
}

void command_slider_dec(MenuContext& ctx, std::int32_t index) {
    adjust_slider(ctx, index, -1.0f);
}

void cycle_option(MenuContext& ctx, std::int32_t index, int direction) {
    EntryBinding* binding = get_entry_binding(ctx, index);
    if (!binding || !binding->entry.value || !binding->entry.metadata)
        return;
    if (std::string* sv = std::get_if<std::string>(binding->entry.value)) {
        const auto& options = binding->entry.metadata->widget.options;
        if (options.empty())
            return;
        int current = 0;
        for (std::size_t i = 0; i < options.size(); ++i) {
            if (options[i].value == *sv) {
                current = static_cast<int>(i);
                break;
            }
        }
        current = (current + direction + static_cast<int>(options.size())) % static_cast<int>(options.size());
        *sv = options[static_cast<std::size_t>(current)].value;
        persist_binding(*binding, ctx.state<SettingsCategoryState>().profile_settings);
    }
}

void command_option_prev(MenuContext& ctx, std::int32_t index) {
    cycle_option(ctx, index, -1);
}

void command_option_next(MenuContext& ctx, std::int32_t index) {
    cycle_option(ctx, index, +1);
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<SettingsCategoryState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
}

std::string format_value(const SettingsValue& value) {
    if (const int* iv = std::get_if<int>(&value))
        return *iv != 0 ? "On" : "Off";
    if (const float* fv = std::get_if<float>(&value)) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(*fv));
        return buffer;
    }
    if (const std::string* sv = std::get_if<std::string>(&value))
        return *sv;
    return {};
}

void refresh_entries(SettingsCategoryState& st, const SettingsCatalog& catalog) {
    st.entries.clear();
    auto it = catalog.categories.find(st.tag);
    if (it == catalog.categories.end())
        return;
    for (const auto& entry : it->second) {
        if (entry.metadata)
            st.entries.push_back({entry});
    }
    std::sort(st.entries.begin(), st.entries.end(), [](const EntryBinding& a, const EntryBinding& b) {
        if (a.entry.metadata->order != b.entry.metadata->order)
            return a.entry.metadata->order < b.entry.metadata->order;
        return a.entry.metadata->label < b.entry.metadata->label;
    });
}

MenuWidget make_setting_widget(const EntryBinding& binding,
                               WidgetId id,
                               UILayoutObjectId slot,
                               int entry_index,
                               std::vector<std::string>& label_cache) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Card;
    w.label = binding.entry.metadata ? binding.entry.metadata->label.c_str() : "";
    w.secondary = binding.entry.metadata ? binding.entry.metadata->description.c_str() : "";
    if (!binding.entry.value || !binding.entry.metadata)
        return w;

    const SettingWidgetDesc& desc = binding.entry.metadata->widget;
    switch (desc.kind) {
        case SettingWidgetKind::Toggle: {
            w.type = WidgetType::Toggle;
            bool on = false;
            if (const int* iv = std::get_if<int>(binding.entry.value))
                on = (*iv != 0);
            else if (const float* fv = std::get_if<float>(binding.entry.value))
                on = (*fv >= 0.5f);
            label_cache.emplace_back(on ? "On" : "Off");
            w.badge = label_cache.back().c_str();
            MenuAction toggle = MenuAction::run_command(g_cmd_toggle_setting, entry_index);
            w.on_select = toggle;
            w.on_left = toggle;
            w.on_right = toggle;
            break;
        }
        case SettingWidgetKind::Slider: {
            w.type = WidgetType::Slider1D;
            w.min = desc.min;
            w.max = desc.max;
            label_cache.push_back(format_value(*binding.entry.value));
            w.badge = label_cache.back().c_str();
            w.on_left = MenuAction::run_command(g_cmd_slider_dec, entry_index);
            w.on_right = MenuAction::run_command(g_cmd_slider_inc, entry_index);
            break;
        }
        case SettingWidgetKind::Option: {
            w.type = WidgetType::OptionCycle;
            label_cache.push_back(format_value(*binding.entry.value));
            w.badge = label_cache.back().c_str();
            w.on_left = MenuAction::run_command(g_cmd_option_prev, entry_index);
            w.on_right = MenuAction::run_command(g_cmd_option_next, entry_index);
            break;
        }
        default:
            label_cache.push_back(format_value(*binding.entry.value));
            w.badge = label_cache.back().c_str();
            break;
    }
    return w;
}

BuiltScreen build_settings_category(MenuContext& ctx) {
    auto tag_it = g_screen_to_tag.find(ctx.screen_id);
    if (tag_it == g_screen_to_tag.end()) {
        BuiltScreen built;
        built.layout = UILayoutID::SETTINGS_SCREEN;
        return built;
    }

    SettingsCatalog catalog = build_settings_catalog(ctx.player_index);
    auto& st = ctx.state<SettingsCategoryState>();
    st.tag = tag_it->second;
    st.profile_settings = catalog.profile_settings;

    refresh_entries(st, catalog);

    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    static std::vector<std::string> label_cache;
    widgets.clear();
    frame_actions.clear();
    label_cache.clear();

    const int total_entries = static_cast<int>(st.entries.size());
    st.total_pages = std::max(1, (total_entries + kSettingsPerPage - 1) / kSettingsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);

    widgets.reserve(kSettingsPerPage + 8);
    widgets.push_back(make_label_widget(400, SettingsObjectID::TITLE, st.tag.c_str()));

    st.status_text = std::to_string(total_entries) + " items";
    MenuWidget status_label = make_label_widget(401, SettingsObjectID::STATUS, st.status_text.c_str());
    status_label.label = st.status_text.c_str();
    widgets.push_back(status_label);
    widgets.push_back(make_label_widget(402, SettingsObjectID::SEARCH, "Search coming soon"));

    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    MenuWidget page_label = make_label_widget(403, SettingsObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(404, SettingsObjectID::PREV, "<", prev_action);
    prev_btn.on_left = prev_action;
    prev_btn.on_right = next_action;
    MenuWidget next_btn = make_button_widget(405, SettingsObjectID::NEXT, ">", next_action);
    next_btn.on_left = prev_action;
    next_btn.on_right = next_action;
    widgets.push_back(prev_btn);
    widgets.push_back(next_btn);

    std::vector<WidgetId> row_ids;
    row_ids.reserve(kSettingsPerPage);
    for (int i = 0; i < kSettingsPerPage; ++i) {
        int entry_index = st.page * kSettingsPerPage + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(500 + i);
        if (entry_index < total_entries) {
            MenuWidget row =
                make_setting_widget(st.entries[static_cast<std::size_t>(entry_index)], widget_id, slot, entry_index, label_cache);
            row.on_left = prev_action.type != MenuActionType::None ? prev_action : row.on_left;
            row.on_right = next_action.type != MenuActionType::None ? next_action : row.on_right;
            widgets.push_back(row);
            row_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(430, SettingsObjectID::BACK, "Back", MenuAction::pop());
    back_btn.on_left = prev_action;
    back_btn.on_right = next_action;
    widgets.push_back(back_btn);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.frame_actions = MenuActionList{frame_actions};
    built.default_focus = !row_ids.empty()
                              ? row_ids.front()
                              : (prev_action.type != MenuActionType::None ? prev_btn.id
                                                                          : (next_action.type != MenuActionType::None ? next_btn.id
                                                                                                                       : back_btn.id));
    return built;
}

void register_screen_for_tag(MenuScreenId id) {
    if (!es)
        return;
    MenuScreenDef def;
    def.id = id;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<SettingsCategoryState>();
    def.build = build_settings_category;
    es->menu_manager.register_screen(def);
}

} // namespace

MenuScreenId ensure_settings_category_screen(const std::string& tag) {
    auto it = g_tag_to_screen.find(tag);
    if (it != g_tag_to_screen.end())
        return it->second;
    if (g_tag_to_screen.size() >= kMaxCategoryScreens) {
        return g_tag_to_screen.begin()->second;
    }
    MenuScreenId id = MenuScreenID::SETTINGS_CATEGORY_BASE + static_cast<MenuScreenId>(g_tag_to_screen.size());
    g_tag_to_screen.emplace(tag, id);
    g_screen_to_tag.emplace(id, tag);
    register_screen_for_tag(id);
    return id;
}

const std::string* tag_for_settings_screen(MenuScreenId id) {
    auto it = g_screen_to_tag.find(id);
    if (it == g_screen_to_tag.end())
        return nullptr;
    return &it->second;
}

void register_settings_category_screens() {
    if (!es)
        return;
    if (g_cmd_toggle_setting == kMenuIdInvalid)
        g_cmd_toggle_setting = es->menu_commands.register_command(command_toggle_setting);
    if (g_cmd_slider_inc == kMenuIdInvalid)
        g_cmd_slider_inc = es->menu_commands.register_command(command_slider_inc);
    if (g_cmd_slider_dec == kMenuIdInvalid)
        g_cmd_slider_dec = es->menu_commands.register_command(command_slider_dec);
    if (g_cmd_option_prev == kMenuIdInvalid)
        g_cmd_option_prev = es->menu_commands.register_command(command_option_prev);
    if (g_cmd_option_next == kMenuIdInvalid)
        g_cmd_option_next = es->menu_commands.register_command(command_option_next);
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
}
