#include "engine/menu/settings_category_registry.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/globals.hpp"
#include "engine/graphics.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_system_state.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/settings_catalog.hpp"
#include "engine/graphics.hpp"
#include "game/state.hpp"
#include "engine/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {
namespace msi = menu_system_internal;

constexpr std::size_t kMaxCategoryScreens = 48;
constexpr int kSettingsPerPage = 4;
constexpr WidgetId kTitleWidgetId = 400;
constexpr WidgetId kStatusWidgetId = 401;
constexpr WidgetId kSearchWidgetId = 402;
constexpr WidgetId kPageLabelWidgetId = 403;
constexpr WidgetId kPrevButtonId = 404;
constexpr WidgetId kNextButtonId = 405;
constexpr WidgetId kBackButtonId = 430;
constexpr WidgetId kFirstRowWidgetId = 500;
constexpr const char* kWindowModeSettingKey = "gubsy.video.window_mode";
constexpr const char* kFrameCapSettingKey = "gubsy.video.frame_cap";
constexpr const char* kRenderResolutionSettingKey = "gubsy.video.render_resolution";

const char* window_mode_to_value(WindowDisplayMode mode);
bool value_to_window_mode(const std::string& value, WindowDisplayMode& out);
bool parse_slider_option_value(const SettingOption& opt, float& out);
bool parse_resolution_value(const std::string& text, int& out_w, int& out_h);
std::string make_resolution_string(int w, int h);
int parse_int_or(const std::string& text, int fallback);

MenuCommandId g_cmd_toggle_setting = kMenuIdInvalid;
MenuCommandId g_cmd_slider_inc = kMenuIdInvalid;
MenuCommandId g_cmd_slider_dec = kMenuIdInvalid;
MenuCommandId g_cmd_option_prev = kMenuIdInvalid;
MenuCommandId g_cmd_option_next = kMenuIdInvalid;
MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_apply_window_mode = kMenuIdInvalid;
MenuCommandId g_cmd_apply_render_resolution = kMenuIdInvalid;

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
    std::vector<std::string> value_buffers;
    struct ResolutionEditState {
        std::string width_text{"1920"};
        std::string height_text{"1080"};
        bool dirty{false};
    } resolution_edit;
    int resolution_entry_index{-1};
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
        if (!desc.options.empty()) {
            std::vector<float> discrete;
            discrete.reserve(desc.options.size());
            for (const auto& opt : desc.options) {
                float parsed = 0.0f;
                if (parse_slider_option_value(opt, parsed))
                    discrete.push_back(parsed);
            }
            if (!discrete.empty()) {
                int closest = 0;
                float best_diff = std::numeric_limits<float>::max();
                for (int i = 0; i < static_cast<int>(discrete.size()); ++i) {
                    float diff = std::fabs(discrete[static_cast<std::size_t>(i)] - *fv);
                    if (diff < best_diff) {
                        best_diff = diff;
                        closest = i;
                    }
                }
                int target = closest + (direction > 0.0f ? 1 : -1);
                target = std::clamp(target, 0, static_cast<int>(discrete.size()) - 1);
                if (target != closest) {
                    *fv = discrete[static_cast<std::size_t>(target)];
                    persist_binding(*binding, ctx.state<SettingsCategoryState>().profile_settings);
                }
                return;
            }
        }
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
    auto& st = ctx.state<SettingsCategoryState>();
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
        persist_binding(*binding, st.profile_settings);
        if (binding->entry.metadata->key == kRenderResolutionSettingKey &&
            st.resolution_entry_index == index) {
            int rw = 0;
            int rh = 0;
            if (parse_resolution_value(*sv, rw, rh)) {
                st.resolution_edit.width_text = std::to_string(rw);
                st.resolution_edit.height_text = std::to_string(rh);
                glm::ivec2 dims = get_render_dimensions();
                st.resolution_edit.dirty = (rw != dims.x || rh != dims.y);
            }
        }
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

void command_apply_window_mode(MenuContext& ctx, std::int32_t index) {
    EntryBinding* binding = get_entry_binding(ctx, index);
    if (!binding || !binding->entry.metadata || binding->entry.metadata->key != kWindowModeSettingKey)
        return;
    auto& st = ctx.state<SettingsCategoryState>();
    if (std::string* sv = std::get_if<std::string>(binding->entry.value)) {
        WindowDisplayMode mode;
        if (!value_to_window_mode(*sv, mode)) {
            st.status_text = "Unknown display mode";
            return;
        }
        if (set_window_display_mode(mode)) {
            st.status_text = "Display mode applied";
            if (gg)
                gg->window_mode = mode;
        } else {
            st.status_text = "Failed to apply display mode";
        }
    }
}

void command_apply_render_resolution(MenuContext& ctx, std::int32_t index) {
    auto& st = ctx.state<SettingsCategoryState>();
    if (index < 0 || index >= static_cast<int>(st.entries.size()))
        return;
    if (st.resolution_entry_index != index)
        return;
    int width = std::clamp(parse_int_or(st.resolution_edit.width_text, 0), 320, 16384);
    int height = std::clamp(parse_int_or(st.resolution_edit.height_text, 0), 240, 9216);
    std::string value = make_resolution_string(width, height);
    EntryBinding& binding = st.entries[static_cast<std::size_t>(index)];
    if (std::string* sv = std::get_if<std::string>(binding.entry.value)) {
        *sv = value;
        persist_binding(binding, st.profile_settings);
        if (set_render_resolution(width, height)) {
            msi::play_confirm_sound();
            st.status_text = "Render resolution set to " + value;
            st.resolution_edit.width_text = std::to_string(width);
            st.resolution_edit.height_text = std::to_string(height);
            st.resolution_edit.dirty = false;
        } else {
            st.status_text = "Failed to set render resolution";
        }
    }
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

std::string format_slider_display(const SettingWidgetDesc& desc, float value) {
    float shown = value * desc.display_scale + desc.display_offset;
    int precision = std::max(0, desc.display_precision);
    char buffer[64];
    if (precision == 0)
        std::snprintf(buffer, sizeof(buffer), "%.0f", static_cast<double>(shown));
    else
        std::snprintf(buffer, sizeof(buffer), "%.*f", precision, static_cast<double>(shown));
    return buffer;
}

const char* window_mode_to_value(WindowDisplayMode mode) {
    switch (mode) {
        case WindowDisplayMode::Windowed:
            return "windowed";
        case WindowDisplayMode::Borderless:
            return "borderless";
        case WindowDisplayMode::Fullscreen:
            return "fullscreen";
        default:
            return "windowed";
    }
}

bool value_to_window_mode(const std::string& value, WindowDisplayMode& out) {
    if (value == "windowed") {
        out = WindowDisplayMode::Windowed;
        return true;
    }
    if (value == "borderless") {
        out = WindowDisplayMode::Borderless;
        return true;
    }
    if (value == "fullscreen") {
        out = WindowDisplayMode::Fullscreen;
        return true;
    }
    return false;
}

bool parse_resolution_value(const std::string& text, int& out_w, int& out_h) {
    auto xpos = text.find('x');
    if (xpos == std::string::npos)
        return false;
    std::string wstr = text.substr(0, xpos);
    std::string hstr = text.substr(xpos + 1);
    try {
        int w = std::stoi(wstr);
        int h = std::stoi(hstr);
        if (w <= 0 || h <= 0)
            return false;
        out_w = w;
        out_h = h;
        return true;
    } catch (...) {
        return false;
    }
}

std::string make_resolution_string(int w, int h) {
    return std::to_string(w) + "x" + std::to_string(h);
}

int parse_int_or(const std::string& text, int fallback) {
    try {
        if (text.empty())
            return fallback;
        return std::stoi(text);
    } catch (...) {
        return fallback;
    }
}

bool parse_slider_option_value(const SettingOption& opt, float& out) {
    if (opt.value.empty())
        return false;
    char* end_ptr = nullptr;
    float parsed = std::strtof(opt.value.c_str(), &end_ptr);
    if (end_ptr && end_ptr != opt.value.c_str()) {
        out = parsed;
        return true;
    }
    std::string lower = opt.value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower == "unlimited" || lower == "none" || lower == "off") {
        out = 0.0f;
        return true;
    }
    return false;
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

    st.value_buffers.resize(st.entries.size());
    st.resolution_entry_index = -1;

    int editing_entry_index = -1;
    WidgetId editing_widget = menu_system_internal::current_text_widget();
    if (editing_widget != kMenuIdInvalid) {
        if (editing_widget >= kFirstRowWidgetId &&
            editing_widget < kFirstRowWidgetId + kSettingsPerPage) {
            int row = static_cast<int>(editing_widget - kFirstRowWidgetId);
            editing_entry_index = st.page * kSettingsPerPage + row;
        }
    }
    for (std::size_t i = 0; i < st.entries.size(); ++i) {
        const SettingMetadata* meta = st.entries[i].entry.metadata;
        bool is_editing_entry = (editing_entry_index == static_cast<int>(i));
        if (meta && meta->key == kRenderResolutionSettingKey)
            st.resolution_entry_index = static_cast<int>(i);
        if (meta && meta->widget.kind == SettingWidgetKind::Slider && meta->widget.max_text_len > 0 &&
            st.entries[i].entry.value && !is_editing_entry) {
            if (const float* fv = std::get_if<float>(st.entries[i].entry.value))
                st.value_buffers[i] = format_slider_display(meta->widget, *fv);
            else
                st.value_buffers[i].clear();
        } else if (!is_editing_entry) {
            st.value_buffers[i].clear();
        }
    }
    if (st.resolution_entry_index >= 0 && st.resolution_entry_index < static_cast<int>(st.entries.size())) {
        EntryBinding& binding = st.entries[static_cast<std::size_t>(st.resolution_entry_index)];
        bool editing_resolution = (editing_entry_index == st.resolution_entry_index);
        int desired_w = 0;
        int desired_h = 0;
        if (const std::string* sv = std::get_if<std::string>(binding.entry.value)) {
            if (!editing_resolution && parse_resolution_value(*sv, desired_w, desired_h)) {
                st.resolution_edit.width_text = std::to_string(desired_w);
                st.resolution_edit.height_text = std::to_string(desired_h);
            } else if (!editing_resolution) {
                glm::ivec2 dims = get_render_dimensions();
                desired_w = dims.x;
                desired_h = dims.y;
                st.resolution_edit.width_text = std::to_string(desired_w);
                st.resolution_edit.height_text = std::to_string(desired_h);
            }
        }
        glm::ivec2 dims = get_render_dimensions();
        int pending_w = std::clamp(parse_int_or(st.resolution_edit.width_text, dims.x), 320, 16384);
        int pending_h = std::clamp(parse_int_or(st.resolution_edit.height_text, dims.y), 240, 9216);
        st.resolution_edit.dirty = (pending_w != dims.x || pending_h != dims.y);
    }
}

MenuWidget make_setting_widget(const EntryBinding& binding,
                               WidgetId id,
                               UILayoutObjectId slot,
                               int entry_index,
                               std::vector<std::string>& label_cache,
                               std::string* value_buffer,
                               SettingsCategoryState& state) {
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
            if (float* fv = std::get_if<float>(binding.entry.value))
                w.bind_ptr = fv;
            float range = w.max - w.min;
            w.step = desc.step > 0.0f ? desc.step : (range > 0.0f ? range * 0.01f : 0.01f);
            w.display_scale = desc.display_scale;
            w.display_offset = desc.display_offset;
            w.display_precision = desc.display_precision;
            w.on_left = MenuAction::run_command(g_cmd_slider_dec, entry_index);
            w.on_right = MenuAction::run_command(g_cmd_slider_inc, entry_index);
            w.has_discrete_options = !desc.options.empty();
            bool is_frame_cap = binding.entry.metadata &&
                                binding.entry.metadata->key == kFrameCapSettingKey;
            if (const float* fv = std::get_if<float>(binding.entry.value)) {
                label_cache.push_back(format_slider_display(desc, *fv));
                if (is_frame_cap && *fv <= 0.0f)
                    label_cache.back() = "Unlimited";
            } else
                label_cache.push_back(format_value(*binding.entry.value));
            w.badge = label_cache.back().c_str();
            if (value_buffer && desc.max_text_len > 0) {
                if (!menu_system_internal::is_text_edit_widget(id))
                    *value_buffer = label_cache.back();
                w.text_buffer = value_buffer;
                w.text_max_len = desc.max_text_len;
                w.placeholder = is_frame_cap ? "fps" : "value";
                w.badge = nullptr;
                if (menu_system_internal::is_text_edit_widget(id))
                    menu_system_internal::set_active_text_buffer(value_buffer, desc.max_text_len);
            }
            if (is_frame_cap)
                w.show_slider_track = false;
            break;
        }
        case SettingWidgetKind::Option: {
            w.type = WidgetType::OptionCycle;
            label_cache.push_back(format_value(*binding.entry.value));
            w.badge = label_cache.back().c_str();
            if (std::string* sv = std::get_if<std::string>(binding.entry.value))
                w.bind_ptr = sv;
            if (binding.entry.metadata && binding.entry.metadata->key == kWindowModeSettingKey) {
                const char* applied = gg ? window_mode_to_value(gg->window_mode) : nullptr;
                bool matches = true;
                if (applied && w.bind_ptr)
                    matches = (*static_cast<std::string*>(w.bind_ptr) == applied);
                if (matches) {
                    w.style.bg_r = 22;
                    w.style.bg_g = 36;
                    w.style.bg_b = 26;
                    w.style.focus_r = 100;
                    w.style.focus_g = 210;
                    w.style.focus_b = 150;
                    w.badge_color = SDL_Color{140, 220, 150, 255};
                } else {
                    w.style.bg_r = 40;
                    w.style.bg_g = 32;
                    w.style.bg_b = 18;
                    w.style.focus_r = 230;
                    w.style.focus_g = 200;
                    w.style.focus_b = 90;
                    w.badge_color = SDL_Color{240, 205, 120, 255};
                }
                w.on_select = MenuAction::run_command(g_cmd_apply_window_mode, entry_index);
            } else if (binding.entry.metadata && binding.entry.metadata->key == kRenderResolutionSettingKey) {
                if (state.resolution_entry_index == entry_index) {
                    auto apply_dirty_style = [&](bool dirty) {
                        if (dirty) {
                            w.style.bg_r = 48;
                            w.style.bg_g = 34;
                            w.style.bg_b = 18;
                            w.style.focus_r = 230;
                            w.style.focus_g = 200;
                            w.style.focus_b = 90;
                            w.badge_color = SDL_Color{240, 205, 120, 255};
                        } else {
                            w.style.bg_r = 22;
                            w.style.bg_g = 36;
                            w.style.bg_b = 26;
                            w.style.focus_r = 100;
                            w.style.focus_g = 210;
                            w.style.focus_b = 150;
                            w.badge_color = SDL_Color{140, 220, 150, 255};
                        }
                    };
                    apply_dirty_style(state.resolution_edit.dirty);
                    w.text_buffer = &state.resolution_edit.width_text;
                    w.text_max_len = 5;
                    w.placeholder = "Width";
                    w.aux_text_buffer = &state.resolution_edit.height_text;
                    w.aux_text_max_len = 5;
                    w.aux_placeholder = "Height";
                w.select_enters_text = false;
                w.play_select_sound = false;
                w.has_discrete_options = true;
                    std::string badge_text = binding.entry.metadata->label;
                    if (std::string* sv = std::get_if<std::string>(binding.entry.value)) {
                        for (const auto& opt : binding.entry.metadata->widget.options) {
                            if (opt.value == *sv && !opt.label.empty()) {
                                badge_text = opt.label;
                                break;
                            }
                        }
                    }
                    label_cache.push_back(badge_text);
                    w.badge = label_cache.back().c_str();
                    w.on_select = MenuAction::run_command(g_cmd_apply_render_resolution, entry_index);
                }
            }
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
    widgets.push_back(make_label_widget(kTitleWidgetId, SettingsObjectID::TITLE, st.tag.c_str()));

    st.status_text = std::to_string(total_entries) + " items";
    MenuWidget status_label = make_label_widget(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str());
    status_label.label = st.status_text.c_str();
    widgets.push_back(status_label);
    widgets.push_back(make_label_widget(kSearchWidgetId, SettingsObjectID::SEARCH, "Search coming soon"));

    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, SettingsObjectID::PREV, "<", prev_action);
    prev_btn.on_left = MenuAction::none();
    prev_btn.on_right = MenuAction::request_focus(kNextButtonId);
    MenuWidget next_btn = make_button_widget(kNextButtonId, SettingsObjectID::NEXT, ">", next_action);
    next_btn.on_left = MenuAction::request_focus(kPrevButtonId);
    next_btn.on_right = MenuAction::none();
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> row_ids;
    row_ids.reserve(kSettingsPerPage);
    std::size_t rows_offset = widgets.size();
    for (int i = 0; i < kSettingsPerPage; ++i) {
        int entry_index = st.page * kSettingsPerPage + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstRowWidgetId + static_cast<WidgetId>(i));
        if (entry_index < total_entries) {
            MenuWidget row =
                make_setting_widget(st.entries[static_cast<std::size_t>(entry_index)],
                                    widget_id,
                                    slot,
                                    entry_index,
                                    label_cache,
                                    (entry_index < static_cast<int>(st.value_buffers.size())
                                         ? &st.value_buffers[static_cast<std::size_t>(entry_index)]
                                         : nullptr),
                                    st);
            row.nav_left = row.id;
            row.nav_right = row.id;
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

    prev_ref.nav_left = prev_ref.id;
    prev_ref.nav_right = next_ref.id;
    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    prev_ref.nav_up = prev_ref.id;
    next_ref.nav_up = next_ref.id;
    prev_ref.nav_down = (first_row_id != kMenuIdInvalid) ? first_row_id : back_ref.id;
    next_ref.nav_down = (first_row_id != kMenuIdInvalid) ? first_row_id : back_ref.id;

    for (std::size_t i = 0; i < row_ids.size(); ++i) {
        MenuWidget& row = widgets[rows_offset + i];
        row.nav_left = row.id;
        row.nav_right = row.id;
        row.nav_up = (i == 0) ? prev_ref.id : row_ids[i - 1];
        row.nav_down = (i + 1 < row_ids.size()) ? row_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_left = back_ref.id;
    back_ref.nav_right = back_ref.id;
    back_ref.nav_up = (last_row_id != kMenuIdInvalid) ? last_row_id : prev_ref.id;

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
    if (g_cmd_apply_window_mode == kMenuIdInvalid)
        g_cmd_apply_window_mode = es->menu_commands.register_command(command_apply_window_mode);
    if (g_cmd_apply_render_resolution == kMenuIdInvalid)
        g_cmd_apply_render_resolution = es->menu_commands.register_command(command_apply_render_resolution);
}
