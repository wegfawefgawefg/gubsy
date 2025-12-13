#include "main_menu/editor/editor.hpp"

#include "globals.hpp"
#include "main_menu/menu_internal.hpp"
#include "main_menu/menu_layout.hpp"
#include "main_menu/menu_navgraph.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <array>
#include <string>

namespace {

struct EditorScreenInfo {
    const char* label;
    Page page;
    bool supports_layout;
    bool supports_nav;
};

const std::vector<EditorScreenInfo>& editor_screens() {
    static const std::vector<EditorScreenInfo> screens = {
        {"Game Options", LOBBY, true, true},
    };
    return screens;
}

const EditorScreenInfo* find_screen(Page page) {
    const auto& screens = editor_screens();
    for (const auto& info : screens) {
        if (info.page == page)
            return &info;
    }
    return nullptr;
}

const char* page_key(Page page) {
    switch (page) {
        case LOBBY: return "lobby";
        default: return nullptr;
    }
}

std::string page_label(Page page) {
    switch (page) {
        case MAIN: return "Main";
        case SETTINGS: return "Settings";
        case AUDIO: return "Audio";
        case VIDEO: return "Video";
        case CONTROLS: return "Controls";
        case BINDS: return "Binds";
        case BINDS_LOAD: return "Load Preset";
        case PLAYERS: return "Players";
        case MODS: return "Mods Browser";
        case LOBBY: return "Game Options";
        case LOBBY_MODS: return "Lobby Mods";
        default: return "Unknown";
    }
}

bool ensure_layout_mode(Page page) {
    const char* key = page_key(page);
    const EditorScreenInfo* info = find_screen(page);
    if (!key || !info || !info->supports_layout)
        return false;
    if (!layout_edit_enabled())
        layout_toggle_edit_mode();
    layout_set_page(key);
    return true;
}

bool ensure_nav_mode(Page page) {
    const char* key = page_key(page);
    const EditorScreenInfo* info = find_screen(page);
    if (!key || !info || !info->supports_nav)
        return false;
    navgraph_set_page(key);
    if (!navgraph_edit_enabled())
        navgraph_toggle_edit_mode();
    return true;
}

void stop_layout_mode() {
    if (layout_edit_enabled())
        layout_toggle_edit_mode();
}

void stop_nav_mode() {
    if (navgraph_edit_enabled())
        navgraph_toggle_edit_mode();
}

const int kScreensPanelWidthPx = 480;
const int kScreensPanelHeightPx = 320;
const int kScreensItemHeight = 28;

SDL_Rect screens_panel_rect(int width, int height) {
    int panel_w = std::min(kScreensPanelWidthPx, width - 40);
    int panel_h = std::min(kScreensPanelHeightPx, height - 40);
    int x = (width - panel_w) / 2;
    int y = (height - panel_h) / 2;
    return SDL_Rect{x, y, panel_w, panel_h};
}

SDL_Rect screens_item_rect(const SDL_Rect& panel, int index) {
    int start_y = panel.y + 70;
    int y = start_y + index * kScreensItemHeight;
    return SDL_Rect{panel.x + 16, y - 4, panel.w - 32, kScreensItemHeight};
}

} // namespace

void gui_editor_handle_shortcuts(Page current_page) {
    if (!ss)
        return;
    auto& ge = ss->menu.gui_editor;

    auto exit_editor = [&]() {
        ge.active = false;
        ge.mode = decltype(ge.mode)::None;
        ge.view = decltype(ge.view)::Screens;
        ge.status.clear();
        ge.dir_prev.fill(false);
        stop_layout_mode();
        stop_nav_mode();
        ss->menu.page = static_cast<int>(ge.prev_page);
    };

    if (ss->menu_inputs.editor_toggle) {
        if (ge.active) {
            exit_editor();
        } else {
            ge.active = true;
            ge.prev_page = current_page;
            ge.page = current_page;
            ge.mode = decltype(ge.mode)::None;
            ge.view = decltype(ge.view)::Screens;
            ge.selection = 0;
            ge.dir_prev.fill(false);
            ge.status = "Select a screen (W/S, Enter, Esc)";
        }
    }

    if (!ge.active) {
        return;
    } else {
        ge.page = current_page;
    }

    if (ge.view != decltype(ge.view)::Screen)
        return;

    if (ss->menu_inputs.layout_toggle) {
        ge.mode = decltype(ge.mode)::Layout;
        ge.status.clear();
    }
    if (ss->menu_inputs.nav_toggle) {
        ge.mode = decltype(ge.mode)::Nav;
        ge.status.clear();
    }
}

bool gui_editor_consumes_input() {
    return ss && ss->menu.gui_editor.active;
}

void gui_editor_step(const std::vector<ButtonDesc>& buttons, int width, int height) {
    if (!gui_editor_consumes_input())
        return;
    auto& ge = ss->menu.gui_editor;
    const auto& screens = editor_screens();

    auto exit_to_screens = [&]() {
        ge.mode = decltype(ge.mode)::None;
        ge.view = decltype(ge.view)::Screens;
        ge.status = "Select a screen (W/S, Enter, Esc)";
        ge.dir_prev.fill(false);
        stop_layout_mode();
        stop_nav_mode();
        ss->menu.page = static_cast<int>(ge.prev_page);
    };

    auto exit_editor = [&]() {
        ge.active = false;
        ge.mode = decltype(ge.mode)::None;
        ge.view = decltype(ge.view)::Screens;
        ge.status.clear();
        ge.dir_prev.fill(false);
        stop_layout_mode();
        stop_nav_mode();
        ss->menu.page = static_cast<int>(ge.prev_page);
    };

    if (ss->menu_inputs.back) {
        if (ge.mode != decltype(ge.mode)::None) {
            ge.mode = decltype(ge.mode)::None;
            ge.status.clear();
            stop_layout_mode();
            stop_nav_mode();
        } else if (ge.view == decltype(ge.view)::Screen) {
            exit_to_screens();
        } else {
            exit_editor();
        }
        ss->menu.mouse_left_prev = ss->mouse_inputs.left;
        return;
    }

    if (ge.view == decltype(ge.view)::Screens) {
        int count = static_cast<int>(screens.size());
        if (count == 0) {
            ge.status = "No screens available.";
        } else {
            auto pressed = [&](int idx, bool now) {
                std::size_t sidx = static_cast<std::size_t>(idx);
                bool prev = ge.dir_prev[sidx];
                ge.dir_prev[sidx] = now;
                return now && !prev;
            };
            if (pressed(1, ss->menu_inputs.down)) {
                ge.selection = (ge.selection + 1 + count) % count;
            } else if (pressed(0, ss->menu_inputs.up)) {
                ge.selection = (ge.selection - 1 + count) % count;
            }

            SDL_Rect panel = screens_panel_rect(width, height);
            auto point_in = [&](const SDL_Rect& r) {
                int mx = ss->mouse_inputs.pos.x;
                int my = ss->mouse_inputs.pos.y;
                return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
            };
            for (int i = 0; i < count; ++i) {
                SDL_Rect row = screens_item_rect(panel, i);
                if (point_in(row)) {
                    ge.selection = i;
                    break;
                }
            }
            bool mouse_pressed = ss->mouse_inputs.left && !ss->menu.mouse_left_prev;

            auto activate_selection = [&]() {
                ge.selection = std::clamp(ge.selection, 0, count - 1);
                const auto& info = screens[(size_t)ge.selection];
                ge.page = info.page;
                ge.mode = decltype(ge.mode)::None;
                ge.view = decltype(ge.view)::Screen;
                ge.status = "Press Ctrl+L for layout, Ctrl+N for nav. Esc to return.";
                ge.dir_prev.fill(false);
                ss->menu.page = static_cast<int>(info.page);
                ss->menu.focus_id = -1;
                stop_layout_mode();
                stop_nav_mode();
            };

            if (ss->menu_inputs.confirm)
                activate_selection();
            else if (mouse_pressed && point_in(panel))
                activate_selection();
        }
        ss->menu.mouse_left_prev = ss->mouse_inputs.left;
        return;
    }

    if (ge.mode == decltype(ge.mode)::Layout) {
        if (!ensure_layout_mode(ge.page)) {
            ge.status = "Layout editing not available for this page yet.";
            ge.mode = decltype(ge.mode)::None;
            stop_layout_mode();
        } else {
            if (ge.page == LOBBY) {
                lobby_layout_editor_handle(buttons, width, height);
            }
        }
        ss->menu.mouse_left_prev = ss->mouse_inputs.left;
        return;
    }

    stop_layout_mode();

    if (ge.mode == decltype(ge.mode)::Nav) {
        if (!ensure_nav_mode(ge.page)) {
            ge.status = "Nav editing not available for this page yet.";
            ge.mode = decltype(ge.mode)::None;
            stop_nav_mode();
        } else {
            navgraph_handle_editor(buttons, width, height);
        }
        ss->menu.mouse_left_prev = ss->mouse_inputs.left;
        return;
    }

    stop_nav_mode();
    // Neutral editor state: nothing to do except keep mouse state in sync.
    ge.status = "Press Ctrl+L for layout, Ctrl+N for nav. Esc returns to screen list.";
    ss->menu.mouse_left_prev = ss->mouse_inputs.left;
}

void gui_editor_render(SDL_Renderer* r, int width, int height) {
    if (!gui_editor_consumes_input() || !r || !gg || !gg->ui_font)
        return;
    auto& ge = ss->menu.gui_editor;

    auto draw_text = [&](const std::string& text, int x, int y, SDL_Color color) {
        if (text.empty())
            return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf)
            return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex)
            return;
        int tw = 0;
        int th = 0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{x, y, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };

    SDL_Color fg{240, 220, 120, 255};
    int margin = static_cast<int>((MENU_SAFE_MARGIN + 0.01f) * static_cast<float>(width));
    int y = static_cast<int>((MENU_SAFE_MARGIN + 0.01f) * static_cast<float>(height));

    if (ge.view == decltype(ge.view)::Screens) {
        SDL_Rect panel = screens_panel_rect(width, height);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 10, 10, 20, 220);
        SDL_RenderFillRect(r, &panel);
        SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
        SDL_RenderDrawRect(r, &panel);
        draw_text("GUI Editor - Screens", panel.x + 16, panel.y + 12, fg);
        draw_text("Use W/S or Up/Down, click/Enter selects, Esc exits", panel.x + 16, panel.y + 40,
                  SDL_Color{200, 200, 210, 255});
        const auto& screens = editor_screens();
        for (std::size_t i = 0; i < screens.size(); ++i) {
            SDL_Rect row = screens_item_rect(panel, static_cast<int>(i));
            if (static_cast<int>(i) == ge.selection) {
                SDL_SetRenderDrawColor(r, 50, 60, 90, 230);
                SDL_RenderFillRect(r, &row);
            }
            SDL_SetRenderDrawColor(r, 70, 70, 90, 180);
            SDL_RenderDrawRect(r, &row);
            SDL_Color color = (static_cast<int>(i) == ge.selection)
                                  ? SDL_Color{240, 240, 120, 255}
                                  : SDL_Color{200, 200, 210, 255};
            draw_text(screens[i].label, row.x + 8, row.y + 6, color);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        return;
    }

    std::string headline = "GUI Editor - " + page_label(ge.page);
    std::string mode;
    switch (ge.mode) {
        case decltype(ge.mode)::Layout: mode = "Mode: Layout"; break;
        case decltype(ge.mode)::Nav: mode = "Mode: Navigation"; break;
        default: mode = "Mode: Neutral (Ctrl+L/Ctrl+N to begin)"; break;
    }
    draw_text(headline, margin, y, fg);
    draw_text(mode, margin, y + 28, SDL_Color{220, 220, 230, 255});
    if (!ge.status.empty())
        draw_text(ge.status, margin, y + 52, SDL_Color{200, 160, 160, 255});
}
