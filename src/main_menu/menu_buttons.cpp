#include "main_menu/menu_internal.hpp"

#include "config.hpp"
#include "engine/input.hpp"
#include "engine/input_defs.hpp"
#include "globals.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>
#include <algorithm>

std::vector<RectNdc> layout_vlist(RectNdc top_left, float item_w, float item_h, float vgap, int count) {
    std::vector<RectNdc> out;
    out.reserve(static_cast<std::size_t>(std::max(0, count)));
    for (int i = 0; i < count; ++i) {
        RectNdc r{top_left.x, top_left.y + static_cast<float>(i) * (item_h + vgap), item_w, item_h};
        out.push_back(r);
    }
    return out;
}

void ensure_focus_default(const std::vector<ButtonDesc>& btns) {
    if (ss->menu.focus_id >= 0) return;
    for (auto const& b : btns) if (b.enabled) { ss->menu.focus_id = b.id; break; }
}

std::vector<ButtonDesc> build_menu_buttons(int width, int height) {
    (void)width; (void)height;
    std::vector<ButtonDesc> buttons;
    const float item_h = 0.08f;
    const float vgap = 0.02f;
    if (ss->menu.page == MAIN) {
        auto rects = layout_vlist(RectNdc{0.10f, 0.20f, 0.0f, 0.0f}, 0.40f, item_h, vgap, 4);
        buttons.push_back(ButtonDesc{100, rects[0], "Play", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{110, rects[1], "Mods", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{101, rects[2], "Settings", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{102, rects[3], "Quit", ButtonKind::Button, 0.0f, true});
    } else if (ss->menu.page == SETTINGS) {
        auto rects = layout_vlist(RectNdc{0.10f, 0.20f, 0.0f, 0.0f}, 0.50f, item_h, vgap, 6);
        buttons.push_back(ButtonDesc{201, rects[0], "Video", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{200, rects[1], "Audio", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{202, rects[2], "Controls", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{204, rects[3], "Binds", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{205, rects[4], "Players", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{203, rects[5], "Other", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{299, RectNdc{0.00f, 0.82f, 0.20f, 0.08f}, "Back", ButtonKind::Button, 0.0f, true});
    } else if (ss->menu.page == AUDIO) {
        auto rects = layout_vlist(RectNdc{0.10f, 0.20f, 0.0f, 0.0f}, 0.60f, item_h, vgap, 3);
        buttons.push_back(ButtonDesc{300, rects[0], "Master Volume", ButtonKind::Slider, ss->menu.vol_master, true});
        buttons.push_back(ButtonDesc{301, rects[1], "Music Volume", ButtonKind::Slider, ss->menu.vol_music, true});
        buttons.push_back(ButtonDesc{302, rects[2], "SFX Volume", ButtonKind::Slider, ss->menu.vol_sfx, true});
        buttons.push_back(ButtonDesc{399, RectNdc{0.00f, 0.82f, 0.20f, 0.08f}, "Back", ButtonKind::Button, 0.0f, true});
    } else if (ss->menu.page == VIDEO) {
        auto rects = layout_vlist(RectNdc{0.10f, 0.20f, 0.0f, 0.0f}, 0.70f, item_h, vgap, 6);
        buttons.push_back(ButtonDesc{400, rects[0], "Resolution", ButtonKind::OptionCycle, static_cast<float>(ss->menu.video_res_index), true});
        bool windowed = (std::clamp(ss->menu.window_mode_index, 0, 2) == 0);
        buttons.push_back(ButtonDesc{405, rects[1], "Window Size", ButtonKind::OptionCycle, static_cast<float>(ss->menu.window_size_index), windowed});
        buttons.push_back(ButtonDesc{401, rects[2], "Window Mode", ButtonKind::OptionCycle, static_cast<float>(ss->menu.window_mode_index), true});
        buttons.push_back(ButtonDesc{402, rects[3], "VSync", ButtonKind::Toggle, ss->menu.vsync ? 1.0f : 0.0f, true});
        buttons.push_back(ButtonDesc{403, rects[4], "Frame Rate Limit", ButtonKind::OptionCycle, static_cast<float>(ss->menu.frame_limit_index), true});
        buttons.push_back(ButtonDesc{404, rects[5], "UI Scale", ButtonKind::Slider, ss->menu.ui_scale, true});
        bool apply_enabled = false;
        if (gg) {
            static const int kResW[4] = {1280, 1600, 1920, 2560};
            static const int kResH[4] = {720,  900,  1080, 1440};
            int idx = std::clamp(ss->menu.video_res_index, 0, 3);
            bool res_diff = (gg->dims.x != (unsigned)kResW[idx] || gg->dims.y != (unsigned)kResH[idx]);
            Uint32 flags = SDL_GetWindowFlags(gg->window);
            int curr_mode = (flags & SDL_WINDOW_FULLSCREEN) ? 2 : ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 1 : 0);
            bool mode_diff = (std::clamp(ss->menu.window_mode_index, 0, 2) != curr_mode);
            apply_enabled = res_diff || mode_diff;
        }
        buttons.push_back(ButtonDesc{499, RectNdc{0.00f, 0.82f, 0.20f, 0.08f}, "Back", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{498, RectNdc{0.70f, 0.82f, 0.20f, 0.08f}, "Apply", ButtonKind::Button, 0.0f, apply_enabled});
    } else if (ss->menu.page == CONTROLS) {
        auto rects = layout_vlist(RectNdc{0.10f, 0.16f, 0.0f, 0.0f}, 0.70f, item_h, vgap, 4);
        buttons.push_back(ButtonDesc{500, rects[0], "Screen Shake", ButtonKind::Slider, ss->menu.screen_shake, true});
        buttons.push_back(ButtonDesc{501, rects[1], "Mouse Sensitivity", ButtonKind::Slider, ss->menu.mouse_sens, true});
        buttons.push_back(ButtonDesc{502, rects[2], "Controller Sensitivity", ButtonKind::Slider, ss->menu.controller_sens, true});
        buttons.push_back(ButtonDesc{505, rects[3], "Vibration Enabled", ButtonKind::Toggle, ss->menu.vibration_enabled ? 1.0f : 0.0f, true});
        auto ctrl_more = layout_vlist(RectNdc{0.10f, 0.16f + 4 * (item_h + vgap), 0.0f, 0.0f}, 0.70f, item_h, vgap, 1);
        buttons.push_back(ButtonDesc{507, ctrl_more[0], "Vibration Intensity", ButtonKind::Slider, ss->menu.vibration_magnitude, ss->menu.vibration_enabled});
        buttons.push_back(ButtonDesc{599, RectNdc{0.00f, 0.82f, 0.20f, 0.08f}, "Back", ButtonKind::Button, 0.0f, true});
    } else if (ss->menu.page == BINDS) {
        auto rows = layout_vlist(RectNdc{0.10f, 0.16f, 0.0f, 0.0f}, 0.70f, item_h, vgap, 5);
        for (size_t i = 0; i < rows.size(); ++i) {
            buttons.push_back(ButtonDesc{700 + static_cast<int>(i), rows[i], "", ButtonKind::Button, 0.0f, true});
        }
        bool modified = !bindings_equal(ss->input_binds, ss->menu.binds_snapshot);
        float footer_w = 0.16f;
        float footer_gap = 0.02f;
        float footer_y = 0.82f;
        RectNdc footer_rects[5];
        for (int i = 0; i < 5; ++i) {
            footer_rects[i] = RectNdc{0.00f + static_cast<float>(i) * (footer_w + footer_gap), footer_y, footer_w, 0.08f};
        }
        buttons.push_back(ButtonDesc{799, footer_rects[0], "Back", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{791, footer_rects[1], "Reset to Defaults", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{792, footer_rects[2], "Load Preset", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{793, footer_rects[3], "Save As New", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{794, footer_rects[4], "Undo Changes", ButtonKind::Button, 0.0f, modified});
    } else if (ss->menu.page == BINDS_LOAD) {
        auto rows = layout_vlist(RectNdc{0.10f, 0.20f, 0.0f, 0.0f}, 0.60f, item_h, vgap, 5);
        const auto& profiles = ss->menu.binds_profiles;
        int per_page = 5;
        for (size_t i = 0; i < rows.size(); ++i) {
            RectNdc main = rows[i];
            float action_w = 0.12f;
            float action_gap = 0.01f;
            float shrink = action_w * 2.0f + action_gap * 2.0f;
            if (main.w > shrink)
                main.w -= shrink;
            int idx = ss->menu.binds_list_page * per_page + static_cast<int>(i);
            bool has_entry = idx >= 0 && idx < (int)profiles.size();
            bool read_only = has_entry ? profiles[static_cast<size_t>(idx)].read_only : false;
            buttons.push_back(ButtonDesc{720 + static_cast<int>(i), main, "", ButtonKind::Button, 0.0f, has_entry});
            RectNdc rename_rect{main.x + main.w + action_gap, main.y, action_w, main.h};
            RectNdc delete_rect{rename_rect.x + action_w + action_gap, main.y, action_w, main.h};
            buttons.push_back(ButtonDesc{730 + static_cast<int>(i), rename_rect, "Rename", ButtonKind::Button, 0.0f, has_entry && !read_only});
            buttons.push_back(ButtonDesc{740 + static_cast<int>(i), delete_rect, "Delete", ButtonKind::Button, 0.0f, has_entry && !read_only});
        }
        buttons.push_back(ButtonDesc{799, RectNdc{0.00f, 0.82f, 0.20f, 0.08f}, "Back", ButtonKind::Button, 0.0f, true});
    } else if (ss->menu.page == PLAYERS) {
        if ((int)ss->menu.players_presets.size() < ss->menu.players_count)
            ss->menu.players_presets.resize(static_cast<std::size_t>(ss->menu.players_count), ss->menu.binds_current_preset.empty() ? std::string("Default") : ss->menu.binds_current_preset);
        int per_page = 5;
        int start = ss->menu.players_page * per_page;
        int rows = std::min(per_page, ss->menu.players_count - start);
        auto rows_rect = layout_vlist(RectNdc{0.10f, 0.20f, 0.0f, 0.0f}, 0.70f, item_h, vgap, std::max(0, rows));
        for (int i = 0; i < rows; ++i) {
            buttons.push_back(ButtonDesc{900 + i, rows_rect[static_cast<std::size_t>(i)], "Player", ButtonKind::OptionCycle, 0.0f, true});
        }
        buttons.push_back(ButtonDesc{981, RectNdc{0.22f, 0.82f, 0.20f, 0.08f}, "Remove Player", ButtonKind::Button, 0.0f, ss->menu.players_count > 1});
        buttons.push_back(ButtonDesc{980, RectNdc{0.44f, 0.82f, 0.20f, 0.08f}, "Add Player", ButtonKind::Button, 0.0f, true});
        buttons.push_back(ButtonDesc{999, RectNdc{0.00f, 0.82f, 0.20f, 0.08f}, "Back", ButtonKind::Button, 0.0f, true});
    } else if (ss->menu.page == MODS) {
        ensure_mod_catalog_loaded();
        if (ss->menu.mods_visible_indices.empty())
            rebuild_mods_filter();
        buttons.push_back(ButtonDesc{950, RectNdc{0.10f, 0.18f, 0.45f, 0.08f}, "Search Mods", ButtonKind::Button, 0.0f, true});
        bool prev_enabled = ss->menu.mods_catalog_page > 0;
        bool next_enabled = ss->menu.mods_catalog_page + 1 < ss->menu.mods_total_pages;
        buttons.push_back(ButtonDesc{951, RectNdc{0.60f, 0.18f, 0.12f, 0.08f}, "Prev", ButtonKind::Button, 0.0f, prev_enabled});
        buttons.push_back(ButtonDesc{952, RectNdc{0.74f, 0.18f, 0.12f, 0.08f}, "Next", ButtonKind::Button, 0.0f, next_enabled});
        const auto& catalog = menu_mod_catalog();
        const auto& visible = ss->menu.mods_visible_indices;
        int start = ss->menu.mods_catalog_page * kModsPerPage;
        auto cards = layout_vlist(RectNdc{0.10f, 0.28f, 0.0f, 0.0f}, 0.78f, kModsCardHeight, kModsCardGap, kModsPerPage);
        for (int i = 0; i < kModsPerPage; ++i) {
            int idx = start + i;
            if (idx >= (int)visible.size())
                break;
            int catalog_idx = visible[(size_t)idx];
            bool enabled = catalog_idx >= 0 && catalog_idx < (int)catalog.size();
            std::string title = enabled ? catalog[(size_t)catalog_idx].title : std::string();
            ButtonDesc card{900 + i, cards[static_cast<std::size_t>(i)], title, ButtonKind::Button,
                            static_cast<float>(catalog_idx), enabled};
            buttons.push_back(card);
        }
        buttons.push_back(ButtonDesc{998, RectNdc{0.00f, 0.82f, 0.20f, 0.08f}, "Back", ButtonKind::Button, 0.0f, true});
    } else if (ss->menu.page == OTHER) {
        auto rects = layout_vlist(RectNdc{0.10f, 0.20f, 0.0f, 0.0f}, 0.60f, item_h, vgap, 0);
        buttons.push_back(ButtonDesc{899, RectNdc{0.00f, 0.82f, 0.20f, 0.08f}, "Back", ButtonKind::Button, 0.0f, true});
    }
    return buttons;
}
