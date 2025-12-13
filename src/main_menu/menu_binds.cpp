#include "main_menu/menu_internal.hpp"

#include "config.hpp"
#include "engine/input.hpp"
#include "engine/input_defs.hpp"
#include "globals.hpp"
#include "state.hpp"

#include <algorithm>

namespace {

bool profile_name_exists(const std::string& name) {
    if (!ss) return false;
    for (auto const& info : ss->menu.binds_profiles) {
        if (info.name == name)
            return true;
    }
    return false;
}

void show_text_input_error(const std::string& msg) {
    if (!ss) return;
    ss->menu.binds_text_input_error = msg;
    ss->menu.binds_text_input_error_timer = kTextInputErrorDuration;
}

} // namespace

bool set_active_binds_profile(const std::string& name) {
    if (!ss) return false;
    InputBindings tmp{};
    if (!load_input_profile(name, &tmp))
        return false;
    ss->input_binds = tmp;
    ss->menu.binds_current_preset = name;
    ss->menu.binds_snapshot = tmp;
    ss->menu.binds_dirty = false;
    save_active_input_profile_name(name);
    refresh_binds_profiles();
    return true;
}

void delete_binds_profile(const std::string& name) {
    if (binds_profile_is_read_only(name))
        return;
    delete_input_profile(name);
    refresh_binds_profiles();
    if (ss && ss->menu.binds_current_preset == name) {
        set_active_binds_profile(default_input_profile_name());
    }
}

void open_text_input(int mode, int limit, const std::string& initial,
                     const std::string& target, const std::string& title) {
    if (!ss) return;
    ss->menu.binds_text_input_active = true;
    ss->menu.binds_text_input_mode = mode;
    ss->menu.binds_text_input_limit = limit;
    ss->menu.binds_text_input_buffer = initial;
    if ((int)ss->menu.binds_text_input_buffer.size() > ss->menu.binds_text_input_limit)
        ss->menu.binds_text_input_buffer.resize(static_cast<size_t>(ss->menu.binds_text_input_limit));
    ss->menu.binds_text_input_target = target;
    ss->menu.binds_text_input_error.clear();
    ss->menu.binds_text_input_error_timer = 0.0f;
    ss->menu.text_input_title = title;
}

bool binds_profile_is_read_only(const std::string& name) {
    return name.empty() || name == default_input_profile_name();
}

void refresh_binds_profiles() {
    if (!ss) return;
    ss->menu.binds_profiles = list_input_profiles();
}

bool ensure_active_binds_profile_writeable() {
    if (!ss) return false;
    if (!binds_profile_is_read_only(ss->menu.binds_current_preset))
        return true;
    refresh_binds_profiles();
    if ((int)ss->menu.binds_profiles.size() >= kMaxBindsProfiles) {
        ss->menu.binds_toast = "Preset limit reached (50)";
        ss->menu.binds_toast_timer = 1.5f;
        if (aa) play_sound("base:ui_cant");
        return false;
    }
    std::string base = ss->menu.binds_current_preset.empty() ? "Custom" : ss->menu.binds_current_preset + " Copy";
    base = sanitize_profile_name(base);
    if (base.empty()) base = "Custom";
    std::string unique = make_unique_profile_name(base);
    if (unique.empty())
        unique = "Preset";
    if (!save_input_profile(unique, ss->input_binds, false)) {
        ss->menu.binds_toast = "Failed to create preset";
        ss->menu.binds_toast_timer = 1.5f;
        if (aa) play_sound("base:ui_cant");
        return false;
    }
    refresh_binds_profiles();
    ss->menu.binds_current_preset = unique;
    ss->menu.binds_snapshot = ss->input_binds;
    ss->menu.binds_dirty = false;
    save_active_input_profile_name(unique);
    ss->menu.binds_toast = std::string("Created preset: ") + unique;
    ss->menu.binds_toast_timer = 1.2f;
    return true;
}

void autosave_active_binds_profile() {
    if (!ss) return;
    if (ss->menu.binds_current_preset.empty())
        ss->menu.binds_current_preset = default_input_profile_name();
    ensure_input_profiles_dir();
    save_input_profile(ss->menu.binds_current_preset, ss->input_binds, true);
    save_active_input_profile_name(ss->menu.binds_current_preset);
}

void apply_default_bindings_to_active() {
    if (!ss) return;
    InputBindings defaults{};
    if (!load_input_profile(default_input_profile_name(), &defaults))
        defaults = InputBindings{};
    if (!ensure_active_binds_profile_writeable())
        return;
    ss->input_binds = defaults;
    autosave_active_binds_profile();
    ss->menu.binds_dirty = !bindings_equal(ss->input_binds, ss->menu.binds_snapshot);
    ss->menu.binds_toast = "Applied default controls";
    ss->menu.binds_toast_timer = 1.2f;
}

void undo_active_bind_changes() {
    if (!ss) return;
    if (!ensure_active_binds_profile_writeable())
        return;
    ss->input_binds = ss->menu.binds_snapshot;
    autosave_active_binds_profile();
    ss->menu.binds_dirty = false;
    ss->menu.binds_toast = "Reverted changes";
    ss->menu.binds_toast_timer = 1.0f;
}

bool menu_is_text_input_active() {
    return ss && ss->menu.binds_text_input_active;
}

void menu_text_input_append(char c) {
    if (!menu_is_text_input_active()) return;
    if ((int)ss->menu.binds_text_input_buffer.size() >= ss->menu.binds_text_input_limit)
        return;
    ss->menu.binds_text_input_buffer.push_back(c);
}

void menu_text_input_backspace() {
    if (!menu_is_text_input_active()) return;
    if (!ss->menu.binds_text_input_buffer.empty())
        ss->menu.binds_text_input_buffer.pop_back();
}

void menu_text_input_cancel() {
    if (!menu_is_text_input_active()) return;
    ss->menu.binds_text_input_active = false;
    ss->menu.binds_text_input_mode = TEXT_INPUT_NONE;
    ss->menu.binds_text_input_buffer.clear();
    ss->menu.binds_text_input_target.clear();
    ss->menu.binds_text_input_error.clear();
    ss->menu.binds_text_input_error_timer = 0.0f;
    ss->menu.text_input_title.clear();
}

void menu_text_input_submit() {
    if (!menu_is_text_input_active()) return;
    refresh_binds_profiles();
    std::string raw = ss->menu.binds_text_input_buffer;
    bool completed = false;
    if (ss->menu.binds_text_input_mode == TEXT_INPUT_BINDS_NEW) {
        raw = sanitize_profile_name(raw);
        if (raw.empty())
            raw = "Preset";
        if ((int)ss->menu.binds_profiles.size() >= kMaxBindsProfiles) {
            show_text_input_error("Preset limit reached (50)");
            if (aa) play_sound("base:ui_cant");
            return;
        }
        if (profile_name_exists(raw)) {
            show_text_input_error("Preset already exists");
            if (aa) play_sound("base:ui_cant");
            return;
        }
        if (!save_input_profile(raw, ss->input_binds, false)) {
            show_text_input_error("Failed to save preset");
            if (aa) play_sound("base:ui_cant");
            return;
        }
        refresh_binds_profiles();
        ss->menu.binds_current_preset = raw;
        ss->menu.binds_snapshot = ss->input_binds;
        ss->menu.binds_dirty = false;
        save_active_input_profile_name(raw);
        ss->menu.binds_toast = std::string("Saved preset: ") + raw;
        ss->menu.binds_toast_timer = 1.2f;
        completed = true;
    } else if (ss->menu.binds_text_input_mode == TEXT_INPUT_BINDS_RENAME) {
        raw = sanitize_profile_name(raw);
        if (raw.empty())
            raw = "Preset";
        std::string target = ss->menu.binds_text_input_target;
        if (target.empty() || binds_profile_is_read_only(target)) {
            completed = true;
        } else if (raw == target) {
            completed = true;
        } else if (profile_name_exists(raw)) {
            show_text_input_error("Preset already exists");
            if (aa) play_sound("base:ui_cant");
            return;
        } else if (!rename_input_profile(target, raw)) {
            show_text_input_error("Rename failed");
            if (aa) play_sound("base:ui_cant");
            return;
        } else {
            if (ss->menu.binds_current_preset == target) {
                ss->menu.binds_current_preset = raw;
                save_active_input_profile_name(raw);
            }
            ss->menu.binds_toast = std::string("Renamed to: ") + raw;
            ss->menu.binds_toast_timer = 1.2f;
            completed = true;
        }
        refresh_binds_profiles();
    } else if (ss->menu.binds_text_input_mode == TEXT_INPUT_MODS_SEARCH) {
        ss->menu.mods_search_query = trim_copy(raw);
        ss->menu.mods_catalog_page = 0;
        rebuild_mods_filter();
        completed = true;
    }
    if (completed) {
        ss->menu.suppress_confirm_until_release = true;
        menu_text_input_cancel();
    }
}
