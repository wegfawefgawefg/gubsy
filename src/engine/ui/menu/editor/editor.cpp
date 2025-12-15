#include "main_menu/editor/editor.hpp"

#include "globals.hpp"
#include "main_menu/menu_internal.hpp"
#include "main_menu/menu_layout.hpp"
#include "main_menu/menu_objects.hpp"
#include "main_menu/menu_navgraph.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {

using GuiObjectDesc = State::MenuState::GuiObjectDesc;

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

enum class Dir { Up = 0, Down = 1, Left = 2, Right = 3 };

struct ScreenGridLayout {
    std::vector<SDL_Rect> cards;
    int cols{1};
    int rows{1};
    SDL_Rect grid_bounds{0, 0, 0, 0};
};

ScreenGridLayout build_screens_layout(int width, int height, int count) {
    ScreenGridLayout layout;
    if (count <= 0 || width <= 0 || height <= 0) {
        layout.cards.clear();
        layout.cols = layout.rows = 0;
        return layout;
    }
    float cols_f = std::ceil(std::sqrt(static_cast<float>(count)));
    layout.cols = std::max(1, static_cast<int>(cols_f));
    layout.rows = std::max(1, (count + layout.cols - 1) / layout.cols);
    const int margin = static_cast<int>(0.08f * static_cast<float>(std::min(width, height)));
    const int gap = 16;
    int grid_w = width - margin * 2;
    int grid_h = height - margin * 2;
    if (grid_w < 100) grid_w = width;
    if (grid_h < 100) grid_h = height;
    layout.grid_bounds = SDL_Rect{(width - grid_w) / 2, (height - grid_h) / 2, grid_w, grid_h};
    int card_w = std::max(120, (grid_w - (layout.cols - 1) * gap) / layout.cols);
    int card_h = std::max(100, (grid_h - (layout.rows - 1) * gap) / layout.rows);
    layout.cards.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        int row = i / layout.cols;
        int col = i % layout.cols;
        int x = layout.grid_bounds.x + col * (card_w + gap);
        int y = layout.grid_bounds.y + row * (card_h + gap);
        layout.cards.push_back(SDL_Rect{x, y, card_w, card_h});
    }
    return layout;
}

bool pressed_edge(std::array<bool, 4>& cache, Dir dir, bool now) {
    std::size_t idx = static_cast<std::size_t>(static_cast<int>(dir));
    bool prev = cache[idx];
    cache[idx] = now;
    return now && !prev;
}

struct PreviewLayout {
    std::vector<LayoutItem> items;
    int ref_w{1280};
    int ref_h{720};
};

PreviewLayout preview_layout_for_page(Page page) {
    PreviewLayout preview{};
    if (!ss)
        return preview;
    auto backup_cache = ss->menu.layout_cache;
    switch (page) {
        case LOBBY: {
            const int ref_w = 1280;
            const int ref_h = 720;
            layout_set_page("lobby");
            auto buttons = build_lobby_buttons();
            preview.items = lobby_layout_items(buttons, ref_w, ref_h);
            auto object_items = gui_objects_layout_items(ref_w, ref_h);
            preview.items.insert(preview.items.end(), object_items.begin(), object_items.end());
            preview.ref_w = ref_w;
            preview.ref_h = ref_h;
            break;
        }
        default:
            break;
    }
    ss->menu.layout_cache = std::move(backup_cache);
    return preview;
}

SDL_Rect object_pixels_for(const GuiObjectDesc& obj, int width, int height) {
    SDL_FRect pf = ndc_to_pixels(obj.rect, width, height);
    SDL_Rect rect{
        static_cast<int>(std::floor(pf.x)),
        static_cast<int>(std::floor(pf.y)),
        static_cast<int>(std::ceil(pf.w)),
        static_cast<int>(std::ceil(pf.h))
    };
    if (rect.x + rect.w > width) rect.w = width - rect.x;
    if (rect.y + rect.h > height) rect.h = height - rect.y;
    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    return rect;
}

void start_object_mode() {
    if (!ss)
        return;
    ss->menu.objects_edit.enabled = true;
    ss->menu.objects_edit.dragging = false;
    ss->menu.objects_edit.resizing = false;
    ss->menu.objects_edit.selected = -1;
    ss->menu.objects_edit.snap = ss->menu.gui_editor.snap_enabled;
}

void stop_object_mode() {
    if (!ss)
        return;
    if (ss->menu.objects_edit.enabled) {
        ss->menu.objects_edit.enabled = false;
        ss->menu.objects_edit.dragging = false;
        ss->menu.objects_edit.resizing = false;
        ss->menu.objects_edit.rename_target.clear();
        gui_objects_save_if_dirty();
    }
}

void handle_object_mode(int width, int height) {
    if (!ss)
        return;
    auto& edit = ss->menu.objects_edit;
    auto& cache = ss->menu.objects_cache;
    const int handle = 12;
    bool mouse_pressed = ss->mouse_inputs.left && !ss->menu.mouse_left_prev;
    bool mouse_released = !ss->mouse_inputs.left && ss->menu.mouse_left_prev;
    int mx = ss->mouse_inputs.pos.x;
    int my = ss->mouse_inputs.pos.y;
    auto point_in = [&](const SDL_Rect& r) {
        return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
    };
    auto clamp_rect = [&](SDL_Rect& rect) {
        rect.w = std::max(10, rect.w);
        rect.h = std::max(10, rect.h);
        if (rect.x < 0) rect.x = 0;
        if (rect.y < 0) rect.y = 0;
        if (rect.x + rect.w > width) rect.x = width - rect.w;
        if (rect.y + rect.h > height) rect.y = height - rect.h;
    };
    auto apply_snap = [&](int value) {
        if (!edit.snap)
            return value;
        const int step = 8;
        return (value / step) * step;
    };
    if (mouse_pressed && !menu_is_text_input_active()) {
        for (int i = static_cast<int>(cache.items.size()) - 1; i >= 0; --i) {
            SDL_Rect rect = object_pixels_for(cache.items[static_cast<size_t>(i)], width, height);
            SDL_Rect move = SDL_Rect{rect.x - handle - 2, rect.y - handle - 2, handle, handle};
            SDL_Rect resize = SDL_Rect{rect.x + rect.w + 2, rect.y + rect.h + 2, handle, handle};
            if (point_in(move)) {
                edit.selected = i;
                edit.dragging = true;
                edit.resizing = false;
                edit.grab_offset = glm::vec2(static_cast<float>(mx - rect.x),
                                             static_cast<float>(my - rect.y));
                break;
            }
            if (point_in(resize)) {
                edit.selected = i;
                edit.dragging = true;
                edit.resizing = true;
                edit.grab_offset = glm::vec2(static_cast<float>(mx - (rect.x + rect.w)),
                                             static_cast<float>(my - (rect.y + rect.h)));
                break;
            }
            if (point_in(rect)) {
                edit.selected = i;
                edit.dragging = false;
                edit.resizing = false;
                break;
            }
        }
    }
    if (edit.dragging && edit.selected >= 0 && edit.selected < (int)cache.items.size()) {
        auto& obj = cache.items[static_cast<size_t>(edit.selected)];
        SDL_Rect rect = object_pixels_for(obj, width, height);
        if (!edit.resizing) {
            double new_x = static_cast<double>(mx) - static_cast<double>(edit.grab_offset.x);
            double new_y = static_cast<double>(my) - static_cast<double>(edit.grab_offset.y);
            rect.x = apply_snap(static_cast<int>(std::round(new_x)));
            rect.y = apply_snap(static_cast<int>(std::round(new_y)));
        } else {
            double new_br_x = static_cast<double>(mx) - static_cast<double>(edit.grab_offset.x);
            double new_br_y = static_cast<double>(my) - static_cast<double>(edit.grab_offset.y);
            rect.w = apply_snap(static_cast<int>(std::round(new_br_x)) - rect.x);
            rect.h = apply_snap(static_cast<int>(std::round(new_br_y)) - rect.y);
        }
        clamp_rect(rect);
        obj.rect = gui_objects_rect_from_pixels(rect, width, height);
        cache.dirty = true;
    }
    if (mouse_released && edit.dragging) {
        edit.dragging = false;
        edit.resizing = false;
        gui_objects_save_if_dirty();
    }
    if (ss->menu_inputs.confirm && !menu_is_text_input_active() && !edit.dragging && !edit.resizing) {
        SDL_Rect rect{mx - width / 16, my - height / 20, width / 8, height / 12};
        clamp_rect(rect);
        GuiObjectDesc obj;
        obj.id = std::string("obj") + std::to_string(cache.next_id++);
        obj.label = obj.id;
        obj.rect = gui_objects_rect_from_pixels(rect, width, height);
        cache.items.push_back(obj);
        cache.dirty = true;
        edit.selected = static_cast<int>(cache.items.size()) - 1;
        gui_objects_save_if_dirty();
    }
    if (ss->menu_inputs.delete_key && edit.selected >= 0 && edit.selected < (int)cache.items.size()) {
        cache.items.erase(cache.items.begin() + edit.selected);
        cache.dirty = true;
        edit.selected = std::min(edit.selected, static_cast<int>(cache.items.size()) - 1);
        gui_objects_save_if_dirty();
    } else if (ss->menu_inputs.duplicate_key && edit.selected >= 0 && edit.selected < (int)cache.items.size()) {
        auto obj = cache.items[static_cast<size_t>(edit.selected)];
        SDL_Rect rect = object_pixels_for(obj, width, height);
        rect.x += 16;
        rect.y += 16;
        clamp_rect(rect);
        obj.id = std::string("obj") + std::to_string(cache.next_id++);
        obj.rect = gui_objects_rect_from_pixels(rect, width, height);
        cache.items.push_back(obj);
        cache.dirty = true;
        edit.selected = static_cast<int>(cache.items.size()) - 1;
        gui_objects_save_if_dirty();
    }
    if (ss->menu_inputs.label_edit && edit.selected >= 0 && edit.selected < (int)cache.items.size() && !menu_is_text_input_active()) {
        auto& obj = cache.items[static_cast<size_t>(edit.selected)];
        edit.rename_target = obj.id;
        open_text_input(TEXT_INPUT_GUI_OBJECT_LABEL, 32, obj.label, obj.id, "Object Label");
    }
}

void render_object_overlay(SDL_Renderer* r, int width, int height) {
    if (!ss || !r)
        return;
    if (!ss->menu.objects_edit.enabled)
        return;
    auto items = gui_objects_pixel_items(width, height);
    int selected = ss->menu.objects_edit.selected;
    auto draw_text = [&](const std::string& text, int x, int y, SDL_Color color) {
        if (!gg || !gg->ui_font || text.empty())
            return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf)
            return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex)
            return;
        int tw=0, th=0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{x, y, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Rect hint{width - 360, 40, 320, 140};
    SDL_SetRenderDrawColor(r, 8, 8, 16, 220);
    SDL_RenderFillRect(r, &hint);
    SDL_SetRenderDrawColor(r, 180, 180, 200, 255);
    SDL_RenderDrawRect(r, &hint);
    int hx = hint.x + 12;
    int hy = hint.y + 10;
    draw_text("Object Mode (stubbed)", hx, hy, SDL_Color{240, 220, 120, 255});
    hy += 24;
    draw_text("Enter: add placeholder at cursor", hx, hy, SDL_Color{210, 210, 225, 255}); hy += 18;
    draw_text("Delete: remove selection", hx, hy, SDL_Color{210, 210, 225, 255}); hy += 18;
    draw_text("D: duplicate selection", hx, hy, SDL_Color{210, 210, 225, 255}); hy += 18;
    draw_text("F/Enter on selection: rename", hx, hy, SDL_Color{210, 210, 225, 255}); hy += 18;
    draw_text("S: snap grid toggle", hx, hy, SDL_Color{210, 210, 225, 255});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& obj = items[i];
        SDL_Color color = (static_cast<int>(i) == selected)
                              ? SDL_Color{250, 210, 120, 255}
                              : SDL_Color{90, 110, 150, 200};
        SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
        SDL_RenderDrawRect(r, &obj.rect);
    }
    if (items.empty()) {
        draw_text("Press Enter to create a placeholder rect.", hint.x, hint.y + hint.h + 18,
                  SDL_Color{240, 200, 200, 255});
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void render_warp_stub(SDL_Renderer* r, int width, int height) {
    if (!r || !gg || !gg->ui_font)
        return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Rect panel{width - 360, height - 180, 320, 140};
    SDL_SetRenderDrawColor(r, 12, 8, 20, 220);
    SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawColor(r, 190, 170, 210, 255);
    SDL_RenderDrawRect(r, &panel);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    auto draw_text = [&](const std::string& text, int x, int y, SDL_Color color) {
        if (!gg->ui_font || text.empty())
            return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf)
            return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex)
            return;
        int tw=0, th=0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{x, y, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };
    int tx = panel.x + 12;
    int ty = panel.y + 10;
    draw_text("Warp Mode (stubbed)", tx, ty, SDL_Color{240, 220, 120, 255});
    ty += 24;
    draw_text("Eventually: connect widgets across screens.", tx, ty, SDL_Color{210, 210, 225, 255});
    ty += 18;
    draw_text("For now this is a placeholder so the hotkey", tx, ty, SDL_Color{210, 210, 225, 255});
    ty += 18;
    draw_text("is visible and debugging is easier.", tx, ty, SDL_Color{210, 210, 225, 255});
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
            ge.status = "Screens: WASD to move, Enter/click to open, Esc to exit.";
        }
    }

    if (!ge.active) {
        return;
    } else {
        ge.page = current_page;
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
        ge.status = "Screens: WASD to move, Enter/click to open, Esc to exit.";
        ge.dir_prev.fill(false);
        stop_layout_mode();
        stop_nav_mode();
        stop_object_mode();
        gui_objects_save_if_dirty();
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
        stop_object_mode();
        gui_objects_save_if_dirty();
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

    // Handle help and snap toggles in both Screens and Screen views
    bool status_was_set = false;
    if (ss->menu_inputs.help_toggle) {
        ge.show_help = !ge.show_help;
        ge.status = ge.show_help ? "Help shown." : "Help hidden.";
        status_was_set = true;
    }
    if (ss->menu_inputs.snap_toggle) {
        ge.snap_enabled = !ge.snap_enabled;
        ss->menu.layout_edit.snap = ge.snap_enabled;
        ss->menu.objects_edit.snap = ge.snap_enabled;
        ge.status = ge.snap_enabled ? "Snap enabled." : "Snap disabled.";
        status_was_set = true;
    }

    if (ge.view == decltype(ge.view)::Screens) {
        int count = static_cast<int>(screens.size());
        if (ge.selection >= count)
            ge.selection = std::max(0, count - 1);
        if (count == 0) {
            ge.status = "No screens available.";
        } else {
            ScreenGridLayout layout = build_screens_layout(width, height, count);
            auto within = [&](const SDL_Rect& r) {
                int mx = ss->mouse_inputs.pos.x;
                int my = ss->mouse_inputs.pos.y;
                return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
            };
            for (int i = 0; i < count && i < (int)layout.cards.size(); ++i) {
                if (within(layout.cards[(size_t)i])) {
                    ge.selection = i;
                    break;
                }
            }
            bool mouse_pressed = ss->mouse_inputs.left && !ss->menu.mouse_left_prev;

            auto try_move = [&](Dir dir) {
                if (!pressed_edge(ge.dir_prev, dir, (dir == Dir::Up) ? ss->menu_inputs.up :
                                                     (dir == Dir::Down) ? ss->menu_inputs.down :
                                                     (dir == Dir::Left) ? ss->menu_inputs.left :
                                                                          ss->menu_inputs.right))
                    return;
                if (count <= 0)
                    return;
                int idx = ge.selection;
                int col = layout.cols > 0 ? idx % layout.cols : 0;
                int row = layout.cols > 0 ? idx / layout.cols : 0;
                if (dir == Dir::Left) {
                    if (col > 0)
                        ge.selection = idx - 1;
                } else if (dir == Dir::Right) {
                    if (col + 1 < layout.cols && idx + 1 < count)
                        ge.selection = idx + 1;
                } else if (dir == Dir::Up) {
                    if (row > 0)
                        ge.selection = std::max(0, idx - layout.cols);
                } else if (dir == Dir::Down) {
                    if (row + 1 < layout.rows) {
                        int next = idx + layout.cols;
                        if (next < count)
                            ge.selection = next;
                    }
                }
            };
            try_move(Dir::Up);
            try_move(Dir::Down);
            try_move(Dir::Left);
            try_move(Dir::Right);

            auto activate_selection = [&]() {
                ge.selection = std::clamp(ge.selection, 0, count - 1);
                const auto& info = screens[(size_t)ge.selection];
                ge.page = info.page;
                ge.mode = decltype(ge.mode)::None;
                ge.view = decltype(ge.view)::Screen;
                ge.status = "Hotkeys: L=Layout, N=Nav, O=Objects, Z=Warp, H=Help, X=Snap. Esc to return.";
                if (const char* key = page_key(info.page)) {
                    layout_set_page(key);
                    gui_objects_set_page(key);
                }
                ge.dir_prev.fill(false);
                ss->menu.page = static_cast<int>(info.page);
                ss->menu.focus_id = -1;
                stop_layout_mode();
                stop_nav_mode();
            };

            if (ss->menu_inputs.confirm)
                activate_selection();
            else if (mouse_pressed && within(layout.grid_bounds))
                activate_selection();
        }
        ss->menu.mouse_left_prev = ss->mouse_inputs.left;
        return;
    }

    auto leave_current_mode = [&]() {
        switch (ge.mode) {
            case decltype(ge.mode)::Layout: stop_layout_mode(); break;
            case decltype(ge.mode)::Nav: stop_nav_mode(); break;
            case decltype(ge.mode)::Object: stop_object_mode(); break;
            default: break;
        }
        ge.mode = decltype(ge.mode)::None;
    };
    auto switch_mode = [&](decltype(ge.mode) target) {
        if (ge.mode == target)
            return;
        leave_current_mode();
        ge.mode = target;
        if (target == decltype(ge.mode)::Object)
            start_object_mode();
        ge.dir_prev.fill(false);
    };

    if (ss->menu_inputs.layout_toggle) {
        switch_mode(decltype(ge.mode)::Layout);
        ge.status = "Layout mode: drag handles, Ctrl+R resets, Esc exits.";
    } else if (ss->menu_inputs.nav_toggle) {
        switch_mode(decltype(ge.mode)::Nav);
        ge.status = "Navigation mode: WASD sets direction, Space/click targets.";
    } else if (ss->menu_inputs.object_toggle) {
        switch_mode(decltype(ge.mode)::Object);
        ge.status = "Object mode (stubbed): Enter spawns, Delete removes, D duplicates.";
    } else if (ss->menu_inputs.warp_toggle) {
        switch_mode(decltype(ge.mode)::Warp);
        ge.status = "Warp mode (stubbed): placeholder only.";
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
            if (ss->menu_inputs.layout_reset)
                layout_reset_current();
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
    if (ge.mode == decltype(ge.mode)::Object) {
        if (!ss->menu.objects_edit.enabled)
            start_object_mode();
        ss->menu.objects_edit.snap = ge.snap_enabled;
        handle_object_mode(width, height);
        ss->menu.mouse_left_prev = ss->mouse_inputs.left;
        return;
    }
    stop_object_mode();
    if (ge.mode == decltype(ge.mode)::Warp) {
        ge.status = "Warp mode stubbed out for now.";
    }
    if (ge.mode == decltype(ge.mode)::None && !status_was_set) {
        ge.status = "Hotkeys: L=Layout, N=Nav, O=Objects, Z=Warp, H=Help, X=Snap. Esc returns to screen list.";
    }
    printf("DEBUG END STEP: status='%s', show_help=%d, snap=%d\n", ge.status.c_str(), ge.show_help, ge.snap_enabled);
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
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 5, 5, 8, 200);
        SDL_Rect full{0, 0, width, height};
        SDL_RenderFillRect(r, &full);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        draw_text("GUI Editor - Screens", margin, y, fg);
        draw_text("WASD/Arrows move • Enter/click opens • Esc exits", margin, y + 26,
                  SDL_Color{200, 200, 210, 255});
        const auto& screens = editor_screens();
        ScreenGridLayout layout = build_screens_layout(width, height, static_cast<int>(screens.size()));
        SDL_Color border{140, 140, 160, 255};
        for (std::size_t i = 0; i < layout.cards.size() && i < screens.size(); ++i) {
            SDL_Rect card = layout.cards[i];
            bool selected = static_cast<int>(i) == ge.selection;
            SDL_Color fill = selected ? SDL_Color{50, 60, 100, 240} : SDL_Color{25, 28, 36, 220};
            SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
            SDL_RenderFillRect(r, &card);
            SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
            SDL_RenderDrawRect(r, &card);
            draw_text(screens[i].label, card.x + 12, card.y + 10,
                      selected ? SDL_Color{240, 240, 140, 255} : SDL_Color{210, 210, 220, 255});
            SDL_Rect preview{card.x + 12, card.y + 36, card.w - 24, card.h - 48};
            SDL_SetRenderDrawColor(r, 18, 18, 26, 255);
            SDL_RenderFillRect(r, &preview);
            SDL_SetRenderDrawColor(r, 70, 70, 90, 255);
            SDL_RenderDrawRect(r, &preview);
            auto preview_layout = preview_layout_for_page(screens[i].page);
            if (preview_layout.items.empty()) {
                int row_h = std::max(6, preview.h / 6);
                for (int line = 0; line < 4; ++line) {
                    int y0 = preview.y + 6 + line * (row_h + 4);
                    SDL_Rect mini{preview.x + 6, y0, preview.w - 12, row_h};
                    SDL_SetRenderDrawColor(r, selected ? 120 : 80, 120, 150, 200);
                    SDL_RenderFillRect(r, &mini);
                }
            } else {
                float sx = preview_layout.ref_w > 0 ? static_cast<float>(preview.w) / static_cast<float>(preview_layout.ref_w) : 1.0f;
                float sy = preview_layout.ref_h > 0 ? static_cast<float>(preview.h) / static_cast<float>(preview_layout.ref_h) : 1.0f;
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 90, 110, 150, 140);
                for (const auto& item : preview_layout.items) {
                    SDL_Rect br{
                        preview.x + static_cast<int>(std::round(static_cast<float>(item.rect.x) * sx)),
                        preview.y + static_cast<int>(std::round(static_cast<float>(item.rect.y) * sy)),
                        std::max(2, static_cast<int>(std::round(static_cast<float>(item.rect.w) * sx))),
                        std::max(2, static_cast<int>(std::round(static_cast<float>(item.rect.h) * sy)))
                    };
                    br.w = std::min(br.w, preview.x + preview.w - br.x);
                    br.h = std::min(br.h, preview.y + preview.h - br.y);
                    SDL_RenderDrawRect(r, &br);
                }
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            }
        }
        // Render help panel in Screens view before returning
        if (ge.show_help) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_Rect panel{width - 420, 40, 380, 280};
            SDL_SetRenderDrawColor(r, 10, 10, 20, 230);
            SDL_RenderFillRect(r, &panel);
            SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
            SDL_RenderDrawRect(r, &panel);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            int tx = panel.x + 16;
            int ty = panel.y + 12;
            draw_text("Editor Help", tx, ty, SDL_Color{240, 220, 120, 255});
            ty += 28;
            const char* lines[] = {
                "L: Layout mode (Ctrl+R resets)",
                "N: Navigation mode",
                "O: Object mode (Enter spawn, D dup, Delete remove)",
                "Z: Warp mode (coming soon)",
                "X: Toggle snap grid",
                "H: Toggle this help panel",
                "F/Enter in object mode: rename",
                "Esc: Exit mode, Esc twice returns to screens",
                "Ctrl+E: Exit editor"
            };
            for (const char* line : lines) {
                draw_text(line, tx, ty, SDL_Color{210, 210, 225, 255});
                ty += 22;
            }
        }
        return;
    }

    std::string headline = "GUI Editor - " + page_label(ge.page);
    std::string mode;
    switch (ge.mode) {
        case decltype(ge.mode)::Layout: mode = "Mode: Layout"; break;
        case decltype(ge.mode)::Nav: mode = "Mode: Navigation"; break;
        case decltype(ge.mode)::Object: mode = "Mode: Objects (stubbed)"; break;
        case decltype(ge.mode)::Warp: mode = "Mode: Warp (stubbed)"; break;
        default: mode = "Mode: Neutral (L/N/O/W, H for help)"; break;
    }
    draw_text(headline, margin, y, fg);
    draw_text(mode, margin, y + 28, SDL_Color{220, 220, 230, 255});
    if (!ge.status.empty())
        draw_text(ge.status, margin, y + 52, SDL_Color{200, 160, 160, 255});
    if (ge.mode == decltype(ge.mode)::Object && ss && ss->menu.objects_edit.enabled)
        render_object_overlay(r, width, height);
    if (ge.mode == decltype(ge.mode)::Warp)
        render_warp_stub(r, width, height);
    if (ge.mode == decltype(ge.mode)::Object) {
        draw_text("Object Mode (stubbed)", margin, y + 80, SDL_Color{200, 200, 230, 255});
    } else if (ge.mode == decltype(ge.mode)::Warp) {
        draw_text("Warp Mode (stubbed)", margin, y + 80, SDL_Color{200, 200, 230, 255});
    }
    if (ge.show_help) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_Rect panel{width - 420, 40, 380, 260};
        SDL_SetRenderDrawColor(r, 10, 10, 20, 230);
        SDL_RenderFillRect(r, &panel);
        SDL_SetRenderDrawColor(r, 200, 200, 210, 255);
        SDL_RenderDrawRect(r, &panel);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        int tx = panel.x + 16;
        int ty = panel.y + 12;
        draw_text("Editor Help", tx, ty, SDL_Color{240, 220, 120, 255});
        ty += 28;
        const char* lines[] = {
            "L: Layout mode (Ctrl+R resets)",
            "N: Navigation mode",
            "O: Object mode (Enter spawn, D dup, Delete remove)",
            "Z: Warp mode (coming soon)",
            "X: Toggle snap grid",
            "H: Toggle this help panel",
            "F/Enter in object mode: rename",
            "Esc: Exit mode, Esc twice returns to screens",
            "Ctrl+E: Exit editor"
        };
        for (const char* line : lines) {
            draw_text(line, tx, ty, SDL_Color{210, 210, 225, 255});
            ty += 22;
        }
    }
}
