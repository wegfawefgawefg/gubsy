#include "main_menu/menu_internal.hpp"

#include "config.hpp"
#include "engine/input.hpp"
#include "engine/input_defs.hpp"
#include "globals.hpp"
#include "demo_items.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

namespace {
// Simple nav graphs per page
static inline NavNode nav_main_for_id(int id) {
    switch (id) {
        case 100: return NavNode{-1, 110, 100, 100};
        case 110: return NavNode{100, 101, 110, 110};
        case 101: return NavNode{110, 102, 101, 101};
        case 102: return NavNode{101, -1, 102, 102};
        default: return NavNode{-1,-1,-1,-1};
    } 
}
static inline NavNode nav_settings_for_id(int id) {
    switch (id) {
        case 201: return NavNode{-1, 200, 201, 201}; // Video (top)
        case 200: return NavNode{201, 202, 200, 200}; // Audio
        case 202: return NavNode{200, 204, 202, 202}; // Controls
        case 204: return NavNode{202, 205, 204, 204}; // Binds
        case 205: return NavNode{204, 203, 205, 205}; // Players
        case 203: return NavNode{205, 299, 203, 203}; // Other
        case 299: return NavNode{203, -1, 299, 299}; // Back
        default: return NavNode{-1,-1,-1,-1};
    }
}

static inline NavNode nav_audio_for_id(int id) {
    switch (id) {
        case 300: return NavNode{-1, 301, 300, 300};
        case 301: return NavNode{300, 302, 301, 301};
        case 302: return NavNode{301, 399, 302, 302};
        case 399: return NavNode{302, -1, 399, 399};
        default: return NavNode{-1,-1,-1,-1};
    }
}
static inline NavNode nav_video_for_id(int id) {
    switch (id) {
        case 400: return NavNode{-1, 405, 400, 400};
        case 405: return NavNode{400, 401, 405, 405};
        case 401: return NavNode{405, 402, 401, 401};
        case 402: return NavNode{401, 403, 402, 402};
        case 403: return NavNode{402, 404, 403, 403};
        case 404: return NavNode{403, 499, 404, 404};
        case 499: return NavNode{404, -1, 499, 498}; // Back ↔ Apply horizontally
        case 498: return NavNode{404, -1, 499, 498};
        default: return NavNode{-1,-1,-1,-1};
    }
}
static inline NavNode nav_controls_for_id(int id) {
    switch (id) {
        case 500: return NavNode{-1, 501, 500, 500};
        case 501: return NavNode{500, 502, 501, 501};
        case 502: return NavNode{501, 505, 502, 502};
        case 505: return NavNode{502, 507, 505, 505};
        case 507: return NavNode{505, 506, 507, 507};
        case 506: return NavNode{507, 599, 506, 506};
        case 599: return NavNode{506, -1, 599, 599};
        default: return NavNode{-1,-1,-1,-1};
    }
}

static inline bool is_audio_slider(int id) {
    return id == 300 || id == 301 || id == 302;
}

static inline int audio_slider_index(int id) {
    switch (id) {
        case 300: return 0;
        case 301: return 1;
        case 302: return 2;
        default: return -1;
    }
}

static float* audio_slider_value_ptr(int slider_id) {
    if (!ss) return nullptr;
    switch (slider_id) {
        case 300: return &ss->menu.vol_master;
        case 301: return &ss->menu.vol_music;
        case 302: return &ss->menu.vol_sfx;
        default: return nullptr;
    }
}

static void set_audio_slider_anchor(int slider_id, float value) {
    if (!ss) return;
    int idx = audio_slider_index(slider_id);
    if (idx < 0) return;
    ss->menu.audio_slider_preview_anchor[idx] = value;
    ss->menu.audio_slider_preview_anchor_valid[idx] = true;
}

static constexpr float kAudioSliderPreviewCooldown = 0.12f;

static void play_audio_slider_preview(int slider_id, float prev_value, float new_value, SliderPreviewEvent evt) {
    if (!aa || !ss) return;
    int idx = audio_slider_index(slider_id);
    if (idx < 0) return;
    if (!ss->menu.audio_slider_preview_anchor_valid[idx]) {
        ss->menu.audio_slider_preview_anchor[idx] = prev_value;
        ss->menu.audio_slider_preview_anchor_valid[idx] = true;
    }
    const char* sound = nullptr;
    if (evt == SliderPreviewEvent::Drag) {
        float anchor = ss->menu.audio_slider_preview_anchor[idx];
        float diff = new_value - anchor;
        if (std::fabs(diff) < 0.004f)
            return;
        float& cooldown = ss->menu.audio_slider_preview_cooldown[idx];
        if (cooldown > 0.0f)
            return;
        sound = (diff >= 0.0f) ? "base:ui_right" : "base:ui_left";
        cooldown = kAudioSliderPreviewCooldown;
        ss->menu.audio_slider_preview_anchor[idx] = new_value;
    } else {
        sound = "base:ui_confirm";
        ss->menu.audio_slider_preview_anchor[idx] = new_value;
        ss->menu.audio_slider_preview_anchor_valid[idx] = true;
    }
    if (sound)
        play_sound(sound);
}

static void flush_audio_settings_if_dirty() {
    if (ss && ss->menu.audio_settings_dirty) {
        save_audio_settings_to_ini("config/audio.ini");
        ss->menu.audio_settings_dirty = false;
    }
}

static bool apply_audio_slider_value(int slider_id, float value) {
    if (!ss)
        return false;
    float* target = audio_slider_value_ptr(slider_id);
    if (!target)
        return false;
    float clamped = std::clamp(value, 0.0f, 1.0f);
    bool changed = std::fabs(*target - clamped) > 0.0005f;
    *target = clamped;
    if (changed)
        ss->menu.audio_settings_dirty = true;
    return changed;
}
static inline NavNode nav_binds_for_id(int id) {
    // Keys page rows (700..704), then footer buttons in order:
    // 799 Back, 791 Reset, 792 Load, 794 Undo
    if (id >= 700 && id <= 704) {
        int up = (id == 700) ? -1 : id - 1;
        int down = (id == 704) ? 799 : id + 1;
        return NavNode{up, down, id, id};
    }
    switch (id) {
        case 799: return NavNode{704, -1, 799, 791};
        case 791: return NavNode{704, -1, 799, 792};
        case 792: return NavNode{704, -1, 791, 793};
        case 793: return NavNode{704, -1, 792, 794};
        case 794: return NavNode{704, -1, 793, 794};
        default: return NavNode{-1,-1,-1,-1};
    }
}
static inline NavNode nav_binds_list_for_id(int id) {
    if (id >= 720 && id <= 724) {
        int up = (id == 720) ? -1 : id - 1;
        int down = (id == 724) ? 799 : id + 1;
        int right = 730 + (id - 720);
        return NavNode{up, down, id, right};
    }
    if (id >= 730 && id <= 734) {
        int row = id - 730;
        int up = (id == 730) ? -1 : id - 1;
        int down = (id == 734) ? 799 : id + 1;
        int left = 720 + row;
        int right = 740 + row;
        return NavNode{up, down, left, right};
    }
    if (id >= 740 && id <= 744) {
        int row = id - 740;
        int up = (id == 740) ? -1 : id - 1;
        int down = (id == 744) ? 799 : id + 1;
        int left = 730 + row;
        return NavNode{up, down, left, id};
    }
    switch (id) {
        case 799: return NavNode{724, -1, 799, 799};
        default: return NavNode{-1,-1,-1,-1};
    }
}
static inline NavNode nav_other_for_id(int id) {
    switch (id) {
        case 800: return NavNode{-1, 801, 800, 800};
        case 801: return NavNode{800, 899, 801, 801};
        case 899: return NavNode{801, -1, 899, 899};
        default: return NavNode{-1,-1,-1,-1};
    }
}

static inline NavNode nav_mods_for_id(int id) {
    const int row_base = 900;
    int rows_on_page = 0;
    if (ss) {
        int total = ss->menu.mods_filtered_count;
        int start = ss->menu.mods_catalog_page * kModsPerPage;
        if (total > start)
            rows_on_page = std::min(kModsPerPage, total - start);
    }
    auto row_nav = [&](int row_idx) {
        int up = (row_idx == 0) ? (rows_on_page > 0 ? 950 : 998) : (row_base + row_idx - 1);
        int down = (row_idx == rows_on_page - 1) ? 998 : (row_base + row_idx + 1);
        return NavNode{up, down, row_base + row_idx, row_base + row_idx};
    };
    if (id == 950) {
        int down = (rows_on_page > 0) ? row_base : 998;
        return NavNode{-1, down, 950, 951};
    }
    if (id == 951) {
        int down = (rows_on_page > 0) ? row_base : 998;
        return NavNode{-1, down, 950, 952};
    }
    if (id == 952) {
        int down = (rows_on_page > 0) ? row_base : 998;
        return NavNode{-1, down, 951, 952};
    }
    if (id >= row_base && id < row_base + 32) {
        int row_idx = id - row_base;
        if (row_idx >= 0 && row_idx < rows_on_page)
            return row_nav(row_idx);
        return NavNode{-1,-1,-1,-1};
    }
    if (id == 998) {
        int up = (rows_on_page > 0) ? (row_base + rows_on_page - 1) : 950;
        return NavNode{up, -1, 998, 998};
    }
    return NavNode{-1,-1,-1,-1};
}

} // namespace

void step_menu_logic(int width, int height) {
    auto buttons = build_menu_buttons(width, height);
    ensure_focus_default(buttons);
    bool text_input_active = menu_is_text_input_active();

    // If capturing a bind, ignore all menu navigation and clicks until capture completes (Esc cancels in input.cpp)
    if (ss->menu.capture_action_id >= 0) {
        ss->menu.mouse_left_prev = ss->mouse_inputs.left;
        return;
    }

    auto find_btn = [&](int id) -> const ButtonDesc* {
        for (auto const& b : buttons) {
            if (b.id == id) return &b;
        }
        return nullptr;
    };

    auto nav = [&](int id) -> NavNode {
        switch (ss->menu.page) {
            case MAIN: return nav_main_for_id(id);
            case SETTINGS: return nav_settings_for_id(id);
            case AUDIO: return nav_audio_for_id(id);
            case VIDEO: return nav_video_for_id(id);
            case CONTROLS: return nav_controls_for_id(id);
            case BINDS: return nav_binds_for_id(id);
            case BINDS_LOAD: return nav_binds_list_for_id(id);
            case OTHER: return nav_other_for_id(id);
            case MODS: return nav_mods_for_id(id);
            default: return NavNode{-1,-1,-1,-1};
        }
    };

    // Mouse hover/focus
    bool click = (ss->mouse_inputs.left && !ss->menu.mouse_left_prev);
    for (auto const& b : buttons) {
        SDL_FRect pr = ndc_to_pixels(b.rect, width, height);
        float mx = static_cast<float>(ss->mouse_inputs.pos.x);
        float my = static_cast<float>(ss->mouse_inputs.pos.y);
        bool inside = (mx >= pr.x && mx <= pr.x + pr.w && my >= pr.y && my <= pr.y + pr.h);
        if (inside) {
            // Only let hover steal focus if mouse was the last-used device,
            // but always move focus on click.
            if (ss->menu.mouse_last_used || click) {
                if (ss->menu.focus_id != b.id) { ss->menu.focus_id = b.id; play_sound("base:ui_cursor_move"); }
            }
    if (click && !text_input_active) {
                // Activate
                if (b.id == 100) {
                    ss->mode = modes::PLAYING;
                    if (!demo_items_active())
                        load_demo_item_defs();
                } else if (b.id == 110) {
                    enter_mods_page();
                    ss->menu.page = MODS; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true;
                    if (aa) play_sound("base:ui_confirm");
                } else if (b.id == 101) {
                    ss->menu.page = SETTINGS; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true;
                } else if (b.id == 102) {
                    ss->running = false;
                } else if (b.id == 299) {
                    ss->menu.page = MAIN; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_cant");
                } else if (b.id == 200) {
                    ss->menu.page = AUDIO; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_confirm");
                } else if (b.id == 201) {
                    ss->menu.page = VIDEO; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_confirm");
                } else if (b.id == 202) {
                    ss->menu.page = CONTROLS; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_confirm");
                } else if (b.id == 204) {
                    ss->menu.page = BINDS; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_confirm");
                } else if (b.id == 203) {
                    ss->menu.page = OTHER; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_confirm");
                } else if (b.id == 205) {
                    ss->menu.page = PLAYERS; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_confirm");
                } else if (b.id == 799 && ss->menu.page == BINDS_LOAD) {
                    ss->menu.page = BINDS; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_confirm");
                } else if (b.id == 399 || b.id == 499 || b.id == 599 || b.id == 799 || b.id == 899) {
                    ss->menu.page = SETTINGS; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_cant");
                } else if (ss->menu.page == BINDS && b.id >= 700 && b.id <= 704) {
                    // map row to action for capture
                    static const BindAction order[] = {
                        BA_LEFT, BA_RIGHT, BA_UP, BA_DOWN,
                        BA_USE_LEFT, BA_USE_RIGHT, BA_USE_UP, BA_USE_DOWN, BA_USE_CENTER,
                        BA_PICK_UP, BA_DROP, BA_RELOAD, BA_DASH
                    };
                    int idx = ss->menu.binds_keys_page * 5 + (b.id - 700);
                    if (idx >= 0 && idx < (int)(sizeof(order)/sizeof(order[0]))) ss->menu.capture_action_id = (int)order[idx];
                    ss->menu.ignore_mouse_until_release = true;
                } else if (ss->menu.page == BINDS && b.id == 792) {
                    ss->menu.page = BINDS_LOAD; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true;
                    refresh_binds_profiles();
                    ss->menu.binds_list_page = 0;
                } else if (ss->menu.page == BINDS && b.id == 793) {
                    std::string suggestion = ss->menu.binds_current_preset.empty() ? "Custom" : ss->menu.binds_current_preset;
                    suggestion = sanitize_profile_name(suggestion);
                    if (suggestion.empty()) suggestion = "Preset";
                    open_text_input(TEXT_INPUT_BINDS_NEW, kPresetNameMaxLen, suggestion, std::string{}, "Preset Name");
                    ss->menu.ignore_mouse_until_release = true;
                    if (aa) play_sound("base:ui_confirm");
                } else if (ss->menu.page == BINDS && b.id == 799) {
                    ss->menu.page = SETTINGS; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true; if (aa) play_sound("base:ui_confirm");
                } else if (ss->menu.page == BINDS && b.id == 791) {
                    apply_default_bindings_to_active(); ss->menu.ignore_mouse_until_release = true;
                } else if (ss->menu.page == BINDS && b.id == 794) {
                    undo_active_bind_changes(); ss->menu.ignore_mouse_until_release = true;
                } else if (ss->menu.page == BINDS_LOAD && b.id >= 720 && b.id <= 724 && b.enabled) {
                    int idx = ss->menu.binds_list_page * 5 + (b.id - 720);
                    if (idx >= 0 && idx < (int)ss->menu.binds_profiles.size()) {
                        std::string name = ss->menu.binds_profiles[(size_t)idx].name;
                        if (set_active_binds_profile(name)) {
                            ss->menu.page = BINDS; ss->menu.focus_id = -1;
                            ss->menu.binds_toast = std::string("Loaded preset: ") + name;
                            ss->menu.binds_toast_timer = 1.2f;
                            if (aa) play_sound("base:ui_confirm");
                        }
                    }
                    ss->menu.ignore_mouse_until_release = true;
                } else if (ss->menu.page == BINDS_LOAD && b.id >= 730 && b.id <= 734 && b.enabled) {
                    int idx = ss->menu.binds_list_page * 5 + (b.id - 730);
                    if (idx >= 0 && idx < (int)ss->menu.binds_profiles.size()) {
                        const auto& info = ss->menu.binds_profiles[(size_t)idx];
                        if (!info.read_only)
                            open_text_input(TEXT_INPUT_BINDS_RENAME, kPresetNameMaxLen, info.name, info.name, "Rename Preset");
                    }
                    ss->menu.ignore_mouse_until_release = true;
                } else if (ss->menu.page == BINDS_LOAD && b.id >= 740 && b.id <= 744 && b.enabled) {
                    int idx = ss->menu.binds_list_page * 5 + (b.id - 740);
                    if (idx >= 0 && idx < (int)ss->menu.binds_profiles.size()) {
                        const auto name = ss->menu.binds_profiles[(size_t)idx].name;
                        delete_binds_profile(name);
                        refresh_binds_profiles();
                        ss->menu.binds_toast = std::string("Deleted preset: ") + name;
                        ss->menu.binds_toast_timer = 1.2f;
                        const int per_page = 5;
                        int total_pages = std::max(1, (int)((ss->menu.binds_profiles.size() + per_page - 1) / per_page));
                        ss->menu.binds_list_page = std::min(ss->menu.binds_list_page, total_pages - 1);
                        if (aa) play_sound("base:ui_confirm");
                    }
                    ss->menu.ignore_mouse_until_release = true;
                } else if (ss->menu.page == MODS) {
                    if (b.id == 950) {
                        open_text_input(TEXT_INPUT_MODS_SEARCH, kModsSearchMaxLen,
                                        ss->menu.mods_search_query, std::string{}, "Search Mods");
                        ss->menu.ignore_mouse_until_release = true;
                        if (aa) play_sound("base:ui_confirm");
                    } else if (b.id == 951) {
                        if (ss->menu.mods_catalog_page > 0) {
                            ss->menu.mods_catalog_page = std::max(0, ss->menu.mods_catalog_page - 1);
                            ss->menu.focus_id = -1;
                            if (aa) play_sound("base:ui_left");
                        } else if (aa) {
                            play_sound("base:ui_cant");
                        }
                        ss->menu.ignore_mouse_until_release = true;
                    } else if (b.id == 952) {
                        if (ss->menu.mods_catalog_page + 1 < ss->menu.mods_total_pages) {
                            ss->menu.mods_catalog_page =
                                std::min(ss->menu.mods_total_pages - 1, ss->menu.mods_catalog_page + 1);
                            ss->menu.focus_id = -1;
                            if (aa) play_sound("base:ui_right");
                        } else if (aa) {
                            play_sound("base:ui_cant");
                        }
                        ss->menu.ignore_mouse_until_release = true;
                    } else if (b.id >= 900 && b.id < 900 + kModsPerPage && b.enabled) {
                        int rel = b.id - 900;
                        int abs_idx = ss->menu.mods_catalog_page * kModsPerPage + rel;
                        if (abs_idx >= 0 && abs_idx < (int)ss->menu.mods_visible_indices.size()) {
                            int catalog_idx = ss->menu.mods_visible_indices[(size_t)abs_idx];
                            const auto& catalog = menu_mod_catalog();
                            if (catalog_idx < 0 || catalog_idx >= (int)catalog.size()) {
                                if (aa) play_sound("base:ui_cant");
                            } else {
                                const auto& entry = catalog[(size_t)catalog_idx];
                                bool was_installed = entry.installed;
                                if (entry.required || entry.installing || entry.uninstalling) {
                                    if (aa) play_sound("base:ui_cant");
                                } else {
                                    std::string err;
                                    bool ok = was_installed ? uninstall_mod_by_index(catalog_idx, err)
                                                            : install_mod_by_index(catalog_idx, err);
                                    if (ok) {
                                        if (aa) play_sound(was_installed ? "base:ui_cant" : "base:ui_confirm");
                                        rebuild_mods_filter();
                                    } else {
                                        State::Alert al;
                                        al.text = err.empty() ? "Mod operation failed" : err;
                                        al.ttl = 1.6f;
                                        ss->alerts.push_back(al);
                                        if (aa) play_sound("base:ui_cant");
                                    }
                                }
                            }
                            if (ss->menu.mods_visible_indices.size() <= (size_t)abs_idx)
                                rebuild_mods_filter();
                        }
                        ss->menu.ignore_mouse_until_release = true;
                    } else if (b.id == 998) {
                        ss->menu.page = MAIN; ss->menu.focus_id = -1; ss->menu.ignore_mouse_until_release = true;
                        if (aa) play_sound("base:ui_confirm");
                    }
                } else if (b.kind == ButtonKind::Toggle) {
                    if (b.id == 402) ss->menu.vsync = !ss->menu.vsync;
                    else if (b.id == 503) ss->menu.invert_x = !ss->menu.invert_x;
                    else if (b.id == 504) ss->menu.invert_y = !ss->menu.invert_y;
                    else if (b.id == 505) ss->menu.vibration_enabled = !ss->menu.vibration_enabled;
                } else if (b.kind == ButtonKind::OptionCycle) {
                    if (!ss->menu.ignore_mouse_until_release) {
                        // Determine arrow hit boxes (larger)
                        SDL_Rect rr{(int)std::floor(pr.x), (int)std::floor(pr.y), (int)std::ceil(pr.w), (int)std::ceil(pr.h)};
                        int aw = std::min(36, rr.w / 6);
                        int ah = std::min(28, rr.h - 8);
                        SDL_Rect leftA{rr.x + rr.w - (aw*2 + 8), rr.y + rr.h/2 - ah/2, aw, ah};
                        SDL_Rect rightA{rr.x + rr.w - aw, rr.y + rr.h/2 - ah/2, aw, ah};
                        bool inLeft = (ss->mouse_inputs.pos.x >= leftA.x && ss->mouse_inputs.pos.x <= leftA.x + leftA.w && ss->mouse_inputs.pos.y >= leftA.y && ss->mouse_inputs.pos.y <= leftA.y + leftA.h);
                        bool inRight = (ss->mouse_inputs.pos.x >= rightA.x && ss->mouse_inputs.pos.x <= rightA.x + rightA.w && ss->mouse_inputs.pos.y >= rightA.y && ss->mouse_inputs.pos.y <= rightA.y + rightA.h);
                        if (inLeft) {
                            if (b.id == 400) ss->menu.video_res_index = (ss->menu.video_res_index + 3) % 4;
                            else if (b.id == 401) ss->menu.window_mode_index = (ss->menu.window_mode_index + 2) % 3;
                            else if (b.id == 403) ss->menu.frame_limit_index = (ss->menu.frame_limit_index + 5) % 6;
                            else if (b.id == 405 && b.enabled) ss->menu.window_size_index = (ss->menu.window_size_index + 3) % 4;
                            if (aa) play_sound("base:ui_left");
                        } else if (inRight) {
                            if (b.id == 400) ss->menu.video_res_index = (ss->menu.video_res_index + 1) % 4;
                            else if (b.id == 401) ss->menu.window_mode_index = (ss->menu.window_mode_index + 1) % 3;
                            else if (b.id == 403) ss->menu.frame_limit_index = (ss->menu.frame_limit_index + 1) % 6;
                            else if (b.id == 405 && b.enabled) ss->menu.window_size_index = (ss->menu.window_size_index + 1) % 4;
                            if (aa) play_sound("base:ui_right");
                        } else {
                            // Fallback: click cycles forward
                            if (b.id == 400) ss->menu.video_res_index = (ss->menu.video_res_index + 1) % 4;
                            else if (b.id == 401) ss->menu.window_mode_index = (ss->menu.window_mode_index + 1) % 3;
                            else if (b.id == 403) ss->menu.frame_limit_index = (ss->menu.frame_limit_index + 1) % 6;
                            else if (b.id == 405 && b.enabled) ss->menu.window_size_index = (ss->menu.window_size_index + 1) % 4;
                            if (aa) play_sound("base:ui_right");
                        }
                        // Apply window size immediately when windowed
                        if (b.id == 405 && b.enabled && gg && gg->window) {
                            static const int kResW[4] = {1280, 1600, 1920, 2560};
                            static const int kResH[4] = {720,  900,  1080, 1440};
                            int idx = std::clamp(ss->menu.window_size_index, 0, 3);
                            SDL_SetWindowSize(gg->window, kResW[idx], kResH[idx]);
                            gg->window_dims = glm::uvec2{(unsigned)kResW[idx], (unsigned)kResH[idx]};
                        }
                    }
                } else if (b.id >= 700 && b.id <= 712) {
                    // Enter capture mode for binds
                    ss->menu.capture_action_id = b.id;
                    ss->menu.ignore_mouse_until_release = true;
                }
            }
            // Sliders: click/drag
            if (!text_input_active && b.kind == ButtonKind::Slider) {
                if (ss->mouse_inputs.left) {
                    if (!ss->menu.ignore_mouse_until_release) {
                        bool first_drag_frame = (ss->menu.dragging_id != b.id);
                        ss->menu.dragging_id = b.id;
                        float t = (pr.w > 1.0f) ? (mx - pr.x) / pr.w : 0.0f;
                        t = std::clamp(t, 0.0f, 1.0f);
                        if (is_audio_slider(b.id)) {
                            float* pv = audio_slider_value_ptr(b.id);
                            float prev = pv ? *pv : 0.0f;
                            bool changed = apply_audio_slider_value(b.id, t);
                            float curr = pv ? *pv : prev;
                            if (first_drag_frame)
                                set_audio_slider_anchor(b.id, prev);
                            else if (changed)
                                play_audio_slider_preview(b.id, prev, curr, SliderPreviewEvent::Drag);
                        } else {
                            if (b.id == 500) ss->menu.screen_shake = t;
                            else if (b.id == 501) ss->menu.mouse_sens = t;
                            else if (b.id == 502) ss->menu.controller_sens = t;
                            else if (b.id == 404) ss->menu.ui_scale = t;
                            else if (b.id == 507) ss->menu.vibration_magnitude = t;
                        }
                    }
                }
            }
        }
    }

    // Continue slider drag if active
    if (!ss->mouse_inputs.left) {
        int released_id = ss->menu.dragging_id;
        ss->menu.dragging_id = -1;
        ss->menu.ignore_mouse_until_release = false;
        if (is_audio_slider(released_id)) {
            if (float* pv = audio_slider_value_ptr(released_id)) {
                float val = *pv;
                play_audio_slider_preview(released_id, val, val, SliderPreviewEvent::Release);
            }
            flush_audio_settings_if_dirty();
        }
    }
    if (ss->menu.dragging_id >= 0) {
        int id = ss->menu.dragging_id;
        const ButtonDesc* b = find_btn(id);
        if (b) {
            SDL_FRect pr = ndc_to_pixels(b->rect, width, height);
            float mx = static_cast<float>(ss->mouse_inputs.pos.x);
            float t = (pr.w > 1.0f) ? (mx - pr.x) / pr.w : 0.0f;
            t = std::clamp(t, 0.0f, 1.0f);
            if (is_audio_slider(id)) {
                float* pv = audio_slider_value_ptr(id);
                float prev = pv ? *pv : 0.0f;
                if (pv && !ss->menu.audio_slider_preview_anchor_valid[audio_slider_index(id)])
                    set_audio_slider_anchor(id, prev);
                bool changed = apply_audio_slider_value(id, t);
                float curr = pv ? *pv : prev;
                if (changed)
                    play_audio_slider_preview(id, prev, curr, SliderPreviewEvent::Drag);
            } else {
                if (id == 500) ss->menu.screen_shake = t;
                else if (id == 501) ss->menu.mouse_sens = t;
                else if (id == 502) ss->menu.controller_sens = t;
                else if (id == 404) ss->menu.ui_scale = t;
                else if (id == 507) ss->menu.vibration_magnitude = t;
            }
        }
    }

    // Controller/keyboard navigation with edge+repeat timers
    // First edge fires immediately, then repeats after an initial delay at a fixed interval.
    const float kInitialDelay = 0.35f;   // seconds before repeats start
    const float kRepeatInterval = 0.09f; // seconds between repeats

    // Compute simple left/right edge detection before repeat updates
    bool left_edge_now = (ss->menu.hold_left && !ss->menu.prev_hold_left);
    bool right_edge_now = (ss->menu.hold_right && !ss->menu.prev_hold_right);
    bool left_release_now = (!ss->menu.hold_left && ss->menu.prev_hold_left);
    bool right_release_now = (!ss->menu.hold_right && ss->menu.prev_hold_right);

    auto repeat_fire = [&](bool hold, bool& prev_hold, float& rpt_timer) -> bool {
        bool fire = false;
        if (hold) {
            if (!prev_hold) {
                // Rising edge: fire once and arm initial delay
                fire = true;
                rpt_timer = kInitialDelay;
            } else {
                // Held: count down and fire at repeat interval
                rpt_timer = std::max(0.0f, rpt_timer - ss->dt);
                if (rpt_timer <= 0.0f) {
                    fire = true;
                    rpt_timer += kRepeatInterval;
                }
            }
        } else {
            // Released: clear timer
            rpt_timer = 0.0f;
        }
        prev_hold = hold;
        return fire;
    };

    bool nav_up = repeat_fire(ss->menu.hold_up, ss->menu.prev_hold_up, ss->menu.rpt_up);
    bool nav_down = repeat_fire(ss->menu.hold_down, ss->menu.prev_hold_down, ss->menu.rpt_down);
    bool nav_left = repeat_fire(ss->menu.hold_left, ss->menu.prev_hold_left, ss->menu.rpt_left);
    bool nav_right = repeat_fire(ss->menu.hold_right, ss->menu.prev_hold_right, ss->menu.rpt_right);

    int fid = ss->menu.focus_id;
    if (!text_input_active && fid >= 0) {
        if (nav_up) {
            int n = nav(fid).up; if (n >= 0) { ss->menu.focus_id = n; if (aa) play_sound("base:ui_cursor_move"); }
        } else if (nav_down) {
            int n = nav(fid).down; if (n >= 0) { ss->menu.focus_id = n; if (aa) play_sound("base:ui_cursor_move"); }
        } else if (nav_left) {
            const ButtonDesc* b = find_btn(fid);
            if (b && b->kind == ButtonKind::Slider) {
                float* pv = nullptr;
                if (fid == 300) pv = &ss->menu.vol_master;
                else if (fid == 301) pv = &ss->menu.vol_music;
                else if (fid == 302) pv = &ss->menu.vol_sfx;
                else if (fid == 500) pv = &ss->menu.screen_shake;
                else if (fid == 501) pv = &ss->menu.mouse_sens;
                else if (fid == 502) pv = &ss->menu.controller_sens;
                else if (fid == 404) pv = &ss->menu.ui_scale;
                else if (fid == 507) pv = &ss->menu.vibration_magnitude;
                if (pv) {
                    float new_val = *pv;
                    if (fid == 501 || fid == 502) {
                        double step_val = 0.01; double minv = 0.10, maxv = 10.0; double range = maxv - minv; double tstep = step_val / range;
                        new_val = std::clamp(*pv - (float)tstep, 0.0f, 1.0f);
                    } else {
                        new_val = std::clamp(*pv - 0.05f, 0.0f, 1.0f);
                    }
                    if (is_audio_slider(fid)) {
                        apply_audio_slider_value(fid, new_val);
                        ss->menu.audio_nav_active_id = fid;
                        ss->menu.audio_nav_active_mask |= 0x1; // left input active
                    } else
                        *pv = new_val;
                    if (aa) play_sound("base:ui_left");
                }
            } else if (b && b->kind == ButtonKind::OptionCycle) {
                if (fid == 400) ss->menu.video_res_index = (ss->menu.video_res_index + 3) % 4;
                else if (fid == 401) ss->menu.window_mode_index = (ss->menu.window_mode_index + 2) % 3;
                else if (fid == 403) ss->menu.frame_limit_index = (ss->menu.frame_limit_index + 5) % 6;
                else if (fid == 405 && b->enabled) {
                    ss->menu.window_size_index = (ss->menu.window_size_index + 3) % 4;
                    if (gg && gg->window) {
                        static const int kResW[4] = {1280, 1600, 1920, 2560};
                        static const int kResH[4] = {720,  900,  1080, 1440};
                        int idx = std::clamp(ss->menu.window_size_index, 0, 3);
                        SDL_SetWindowSize(gg->window, kResW[idx], kResH[idx]);
                        gg->window_dims = glm::uvec2{(unsigned)kResW[idx], (unsigned)kResH[idx]};
                    }
                }
                if (aa) play_sound("base:ui_left");
            } else if (b && b->kind == ButtonKind::Toggle) {
                if (fid == 402) ss->menu.vsync = !ss->menu.vsync;
                else if (fid == 503) ss->menu.invert_x = !ss->menu.invert_x;
                else if (fid == 504) ss->menu.invert_y = !ss->menu.invert_y;
                else if (fid == 505) ss->menu.vibration_enabled = !ss->menu.vibration_enabled;
            } else {
                int n = nav(fid).left; if (n >= 0) ss->menu.focus_id = n;
            }
        } else if (nav_right) {
            const ButtonDesc* b = find_btn(fid);
            if (b && b->kind == ButtonKind::Slider) {
                float* pv = nullptr;
                if (fid == 300) pv = &ss->menu.vol_master;
                else if (fid == 301) pv = &ss->menu.vol_music;
                else if (fid == 302) pv = &ss->menu.vol_sfx;
                else if (fid == 500) pv = &ss->menu.screen_shake;
                else if (fid == 501) pv = &ss->menu.mouse_sens;
                else if (fid == 502) pv = &ss->menu.controller_sens;
                else if (fid == 404) pv = &ss->menu.ui_scale;
                else if (fid == 507) pv = &ss->menu.vibration_magnitude;
                if (pv) {
                    float new_val = *pv;
                    if (fid == 501 || fid == 502) {
                        double step_val = 0.01; double minv = 0.10, maxv = 10.0; double range = maxv - minv; double tstep = step_val / range;
                        new_val = std::clamp(*pv + (float)tstep, 0.0f, 1.0f);
                    } else {
                        new_val = std::clamp(*pv + 0.05f, 0.0f, 1.0f);
                    }
                    if (is_audio_slider(fid)) {
                        apply_audio_slider_value(fid, new_val);
                        ss->menu.audio_nav_active_id = fid;
                        ss->menu.audio_nav_active_mask |= 0x2; // right input active
                    } else
                        *pv = new_val;
                    if (aa) play_sound("base:ui_right");
                }
            } else if (b && b->kind == ButtonKind::OptionCycle) {
                if (fid == 400) ss->menu.video_res_index = (ss->menu.video_res_index + 1) % 4;
                else if (fid == 401) ss->menu.window_mode_index = (ss->menu.window_mode_index + 1) % 3;
                else if (fid == 403) ss->menu.frame_limit_index = (ss->menu.frame_limit_index + 1) % 6;
                else if (fid == 405 && b->enabled) {
                    ss->menu.window_size_index = (ss->menu.window_size_index + 1) % 4;
                    if (gg && gg->window) {
                        static const int kResW[4] = {1280, 1600, 1920, 2560};
                        static const int kResH[4] = {720,  900,  1080, 1440};
                        int idx = std::clamp(ss->menu.window_size_index, 0, 3);
                        SDL_SetWindowSize(gg->window, kResW[idx], kResH[idx]);
                        gg->window_dims = glm::uvec2{(unsigned)kResW[idx], (unsigned)kResH[idx]};
                    }
                }
                if (aa) play_sound("base:ui_right");
            } else if (b && b->kind == ButtonKind::Toggle) {
                if (fid == 402) ss->menu.vsync = !ss->menu.vsync;
                else if (fid == 503) ss->menu.invert_x = !ss->menu.invert_x;
                else if (fid == 504) ss->menu.invert_y = !ss->menu.invert_y;
                else if (fid == 505) ss->menu.vibration_enabled = !ss->menu.vibration_enabled;
            } else {
                int n = nav(fid).right; if (n >= 0) ss->menu.focus_id = n;
            }
        }
        if (!text_input_active && ss->menu_inputs.confirm) {
            const ButtonDesc* b = find_btn(ss->menu.focus_id);
            if (b) {
                if (b->id == 100) {
                    ss->mode = modes::PLAYING;
                    if (!demo_items_active())
                        load_demo_item_defs();
                } else if (b->id == 110) {
                    enter_mods_page();
                    ss->menu.page = MODS; ss->menu.focus_id = -1;
                    if (aa) play_sound("base:ui_confirm");
                } else if (b->id == 101) {
                    ss->menu.page = SETTINGS; ss->menu.focus_id = -1;
                } else if (b->id == 102) {
                    ss->running = false;
                } else if (b->id == 299) {
                    ss->menu.page = MAIN; ss->menu.focus_id = -1;
                } else if (b->id == 799) {
                    if (ss->menu.page == BINDS_LOAD) {
                        ss->menu.page = BINDS;
                        ss->menu.focus_id = -1;
                        ss->menu.capture_action_id = -1;
                    } else {
                        ss->menu.page = SETTINGS;
                        ss->menu.focus_id = -1;
                    }
                } else if (b->id == 200) {
                    ss->menu.page = AUDIO; ss->menu.focus_id = -1;
                } else if (b->id == 201) {
                    ss->menu.page = VIDEO; ss->menu.focus_id = -1;
                } else if (b->id == 202) {
                    ss->menu.page = CONTROLS; ss->menu.focus_id = -1;
                } else if (b->id == 203) {
                    ss->menu.page = OTHER; ss->menu.focus_id = -1;
                } else if (b->id == 399 || b->id == 499 || b->id == 599 || b->id == 899) {
                    ss->menu.page = SETTINGS; ss->menu.focus_id = -1;
                } else if (b->id == 204) {
                    ss->menu.page = BINDS; ss->menu.focus_id = -1;
                } else if (b->id == 498 && b->enabled) {
                    if (gg && gg->window) {
                        static const int kResW[4] = {1280, 1600, 1920, 2560};
                        static const int kResH[4] = {720,  900,  1080, 1440};
                        int ridx = std::clamp(ss->menu.video_res_index, 0, 3);
                        gg->dims = glm::uvec2{(unsigned)kResW[ridx], (unsigned)kResH[ridx]};
                        int wmode = std::clamp(ss->menu.window_mode_index, 0, 2);
                        if (wmode == 2) {
                            SDL_SetWindowFullscreen(gg->window, SDL_WINDOW_FULLSCREEN);
                        } else if (wmode == 1) {
                            SDL_SetWindowFullscreen(gg->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        } else {
                            SDL_SetWindowFullscreen(gg->window, 0);
                            SDL_SetWindowBordered(gg->window, SDL_TRUE);
                        }
                    }
                } else if (ss->menu.page == BINDS) {
                    if (b->id >= 700 && b->id <= 704) {
                        static const BindAction order[] = {
                            BA_LEFT, BA_RIGHT, BA_UP, BA_DOWN,
                            BA_USE_LEFT, BA_USE_RIGHT, BA_USE_UP, BA_USE_DOWN, BA_USE_CENTER,
                            BA_PICK_UP, BA_DROP, BA_RELOAD, BA_DASH
                        };
                        int idx = ss->menu.binds_keys_page * 5 + (b->id - 700);
                        if (idx >= 0 && idx < (int)(sizeof(order)/sizeof(order[0])))
                            ss->menu.capture_action_id = (int)order[idx];
                    } else if (b->id == 791 && b->enabled) {
                        apply_default_bindings_to_active();
                        if (aa) play_sound("base:ui_confirm");
                    } else if (b->id == 792 && b->enabled) {
                        ss->menu.page = BINDS_LOAD; ss->menu.focus_id = -1;
                        refresh_binds_profiles();
                        ss->menu.binds_list_page = 0;
                        if (aa) play_sound("base:ui_confirm");
                    } else if (b->id == 793 && b->enabled) {
                        std::string suggestion = ss->menu.binds_current_preset.empty() ? "Custom" : ss->menu.binds_current_preset;
                        suggestion = sanitize_profile_name(suggestion);
                        if (suggestion.empty()) suggestion = "Preset";
                        open_text_input(TEXT_INPUT_BINDS_NEW, kPresetNameMaxLen, suggestion, std::string{}, "Preset Name");
                        if (aa) play_sound("base:ui_confirm");
                    } else if (b->id == 794 && b->enabled) {
                        undo_active_bind_changes();
                        if (aa) play_sound("base:ui_confirm");
                    }
                } else if (ss->menu.page == BINDS_LOAD) {
                    if (b->id >= 720 && b->id <= 724 && b->enabled) {
                        int idx = ss->menu.binds_list_page * 5 + (b->id - 720);
                        if (idx >= 0 && idx < (int)ss->menu.binds_profiles.size()) {
                            std::string name = ss->menu.binds_profiles[(size_t)idx].name;
                            if (set_active_binds_profile(name)) {
                                ss->menu.page = BINDS; ss->menu.focus_id = -1;
                                ss->menu.binds_toast = std::string("Loaded preset: ") + name;
                                ss->menu.binds_toast_timer = 1.2f;
                                if (aa) play_sound("base:ui_confirm");
                            }
                        }
                    } else if (b->id >= 730 && b->id <= 734 && b->enabled) {
                        int idx = ss->menu.binds_list_page * 5 + (b->id - 730);
                        if (idx >= 0 && idx < (int)ss->menu.binds_profiles.size()) {
                            const auto& info = ss->menu.binds_profiles[(size_t)idx];
                            if (!info.read_only)
                                open_text_input(TEXT_INPUT_BINDS_RENAME, kPresetNameMaxLen, info.name, info.name, "Rename Preset");
                        }
                } else if (b->id >= 740 && b->id <= 744 && b->enabled) {
                    int idx = ss->menu.binds_list_page * 5 + (b->id - 740);
                    if (idx >= 0 && idx < (int)ss->menu.binds_profiles.size()) {
                        const auto name = ss->menu.binds_profiles[(size_t)idx].name;
                        delete_binds_profile(name);
                            refresh_binds_profiles();
                            ss->menu.binds_toast = std::string("Deleted preset: ") + name;
                            ss->menu.binds_toast_timer = 1.2f;
                            const int per_page = 5;
                            int total_pages = std::max(1, (int)((ss->menu.binds_profiles.size() + per_page - 1) / per_page));
                        ss->menu.binds_list_page = std::min(ss->menu.binds_list_page, total_pages - 1);
                        if (aa) play_sound("base:ui_confirm");
                    }
                }
                } else if (ss->menu.page == MODS) {
                    if (b->id == 950) {
                        open_text_input(TEXT_INPUT_MODS_SEARCH, kModsSearchMaxLen,
                                        ss->menu.mods_search_query, std::string{}, "Search Mods");
                        if (aa) play_sound("base:ui_confirm");
                    } else if (b->id == 951) {
                        if (ss->menu.mods_catalog_page > 0) {
                            ss->menu.mods_catalog_page = std::max(0, ss->menu.mods_catalog_page - 1);
                            ss->menu.focus_id = -1;
                            if (aa) play_sound("base:ui_left");
                        } else if (aa) {
                            play_sound("base:ui_cant");
                        }
                    } else if (b->id == 952) {
                        if (ss->menu.mods_catalog_page + 1 < ss->menu.mods_total_pages) {
                            ss->menu.mods_catalog_page =
                                std::min(ss->menu.mods_total_pages - 1, ss->menu.mods_catalog_page + 1);
                            ss->menu.focus_id = -1;
                            if (aa) play_sound("base:ui_right");
                        } else if (aa) {
                            play_sound("base:ui_cant");
                        }
                    } else if (b->id >= 900 && b->id < 900 + kModsPerPage && b->enabled) {
                        int rel = b->id - 900;
                        int abs_idx = ss->menu.mods_catalog_page * kModsPerPage + rel;
                        if (abs_idx >= 0 && abs_idx < (int)ss->menu.mods_visible_indices.size()) {
                            int catalog_idx = ss->menu.mods_visible_indices[(size_t)abs_idx];
                            const auto& catalog = menu_mod_catalog();
                            if (catalog_idx < 0 || catalog_idx >= (int)catalog.size()) {
                                if (aa) play_sound("base:ui_cant");
                            } else {
                                const auto& entry = catalog[(size_t)catalog_idx];
                                bool was_installed = entry.installed;
                                if (entry.required || entry.installing || entry.uninstalling) {
                                    if (aa) play_sound("base:ui_cant");
                                } else {
                                    std::string err;
                                    bool ok = was_installed ? uninstall_mod_by_index(catalog_idx, err)
                                                            : install_mod_by_index(catalog_idx, err);
                                    if (ok) {
                                        if (aa) play_sound(was_installed ? "base:ui_cant" : "base:ui_confirm");
                                        rebuild_mods_filter();
                                    } else {
                                        State::Alert al;
                                        al.text = err.empty() ? "Mod operation failed" : err;
                                        al.ttl = 1.6f;
                                        ss->alerts.push_back(al);
                                        if (aa) play_sound("base:ui_cant");
                                    }
                                }
                            }
                        }
                    } else if (b->id == 998) {
                        ss->menu.page = MAIN; ss->menu.focus_id = -1;
                        if (aa) play_sound("base:ui_confirm");
                    }
                }
            }
        }
        }

    // Back button behavior (Esc/Backspace)
    if (text_input_active) {
        if (ss->menu_inputs.back) {
            menu_text_input_cancel();
        }
    } else if (ss->menu_inputs.back) {
        if (aa) play_sound("base:ui_confirm");
        if (ss->menu.page == SETTINGS) {
            ss->menu.page = MAIN; ss->menu.focus_id = -1;
        } else if (ss->menu.page == MODS) {
            ss->menu.page = MAIN; ss->menu.focus_id = -1;
        } else if (ss->menu.page == BINDS_LOAD) {
            ss->menu.page = BINDS; ss->menu.focus_id = -1; ss->menu.capture_action_id = -1;
        } else if (ss->menu.page == AUDIO || ss->menu.page == VIDEO || ss->menu.page == CONTROLS || ss->menu.page == BINDS || ss->menu.page == OTHER) {
            ss->menu.page = SETTINGS; ss->menu.focus_id = -1;
            ss->menu.capture_action_id = -1;
        }
    }

    auto handle_nav_release = [&](bool released, uint8_t bit) {
        if (!released) return;
        if (ss->menu.audio_nav_active_mask & bit) {
            ss->menu.audio_nav_active_mask &= ~bit;
            if (ss->menu.audio_nav_active_mask == 0) {
                ss->menu.audio_nav_active_id = -1;
                flush_audio_settings_if_dirty();
            }
        }
    };
    handle_nav_release(left_release_now, 0x1);
    handle_nav_release(right_release_now, 0x2);
    if (ss->menu.page == BINDS) {
        int total_actions = (int)BindAction::BA_COUNT;
        int per_page = 5;
        int total_pages = std::max(1, (total_actions + per_page - 1) / per_page);
        // Page change: Q/E edges or left/right edges when focus on a binds row
        if (ss->menu.focus_id >= 700 && ss->menu.focus_id <= 704) {
            if (ss->menu_inputs.page_prev || left_edge_now) { ss->menu.binds_keys_page = std::max(0, ss->menu.binds_keys_page - 1); if (aa) play_sound("base:ui_left"); }
            else if (ss->menu_inputs.page_next || right_edge_now) { ss->menu.binds_keys_page = std::min(total_pages - 1, ss->menu.binds_keys_page + 1); if (aa) play_sound("base:ui_right"); }
        }
        int hdr_y = (int)std::lround(MENU_SAFE_MARGIN * static_cast<float>(height)) + 16;
        SDL_Rect prevb{width - 540, hdr_y - 4, 90, 32};
        SDL_Rect nextb{width - 120, hdr_y - 4, 90, 32};
        if (click && !text_input_active) {
            int mx_i = ss->mouse_inputs.pos.x; int my_i = ss->mouse_inputs.pos.y;
            auto in_rect = [&](SDL_Rect rc){ return mx_i >= rc.x && mx_i <= rc.x + rc.w && my_i >= rc.y && my_i <= rc.y + rc.h; };
            if (in_rect(prevb)) { ss->menu.binds_keys_page = std::max(0, ss->menu.binds_keys_page - 1); if (aa) play_sound("base:ui_left"); }
            if (in_rect(nextb)) { ss->menu.binds_keys_page = std::min(total_pages - 1, ss->menu.binds_keys_page + 1); if (aa) play_sound("base:ui_right"); }
        }
    } else if (ss->menu.page == BINDS_LOAD) {
        const auto& profiles = ss->menu.binds_profiles;
        int per_page = 5;
        int total_pages = std::max(1, (int)((profiles.size() + (size_t)per_page - 1) / (size_t)per_page));
        ss->menu.binds_list_page = std::clamp(ss->menu.binds_list_page, 0, total_pages - 1);
        bool focus_profiles = (ss->menu.focus_id >= 720 && ss->menu.focus_id <= 724);
        bool focus_actions = (ss->menu.focus_id >= 730 && ss->menu.focus_id <= 744);
        if (focus_profiles) {
            if (ss->menu_inputs.page_prev || left_edge_now) { ss->menu.binds_list_page = std::max(0, ss->menu.binds_list_page - 1); if (aa) play_sound("base:ui_left"); }
            else if (ss->menu_inputs.page_next || right_edge_now) { ss->menu.binds_list_page = std::min(total_pages - 1, ss->menu.binds_list_page + 1); if (aa) play_sound("base:ui_right"); }
        } else if (focus_actions) {
            if (ss->menu_inputs.page_prev) { ss->menu.binds_list_page = std::max(0, ss->menu.binds_list_page - 1); if (aa) play_sound("base:ui_left"); }
            else if (ss->menu_inputs.page_next) { ss->menu.binds_list_page = std::min(total_pages - 1, ss->menu.binds_list_page + 1); if (aa) play_sound("base:ui_right"); }
        }
        int hdr_y = (int)std::lround(MENU_SAFE_MARGIN * static_cast<float>(height)) + 16;
        SDL_Rect prevb{width - 540, hdr_y - 4, 90, 32};
        SDL_Rect nextb{width - 120, hdr_y - 4, 90, 32};
        if (click && !text_input_active) {
            int mx_i = ss->mouse_inputs.pos.x; int my_i = ss->mouse_inputs.pos.y;
            auto in_rect = [&](SDL_Rect rc){ return mx_i >= rc.x && mx_i <= rc.x + rc.w && my_i >= rc.y && my_i <= rc.y + rc.h; };
            if (in_rect(prevb)) { ss->menu.binds_list_page = std::max(0, ss->menu.binds_list_page - 1); if (aa) play_sound("base:ui_left"); }
            if (in_rect(nextb)) { ss->menu.binds_list_page = std::min(total_pages - 1, ss->menu.binds_list_page + 1); if (aa) play_sound("base:ui_right"); }
        }
    }

    // Pagination with bumpers
    if (ss->menu.page == VIDEO) {
        if (ss->menu_inputs.page_prev) {
            ss->menu.page_index = std::max(0, ss->menu.page_index - 1);
        } else if (ss->menu_inputs.page_next) {
            ss->menu.page_index = std::min(0, ss->menu.page_index + 1);
        }
        // Also allow clicking the Prev/Next buttons
        int hdr_y = (int)std::lround(MENU_SAFE_MARGIN * static_cast<float>(height)) + 16;
        SDL_Rect prevb{width - 540, hdr_y - 4, 90, 32};
        SDL_Rect nextb{width - 120, hdr_y - 4, 90, 32};
        if (click && !text_input_active) {
            int mx_i = ss->mouse_inputs.pos.x; int my_i = ss->mouse_inputs.pos.y;
            auto in_rect = [&](SDL_Rect rc){ return mx_i >= rc.x && mx_i <= rc.x + rc.w && my_i >= rc.y && my_i <= rc.y + rc.h; };
            if (in_rect(prevb)) { ss->menu.page_index = std::max(0, ss->menu.page_index - 1); if (aa) play_sound("base:ui_left"); }
            if (in_rect(nextb)) { ss->menu.page_index = std::min(0, ss->menu.page_index + 1); if (aa) play_sound("base:ui_right"); }
        }
    }

    // Update mouse click edge
    ss->menu.mouse_left_prev = ss->mouse_inputs.left;
}
