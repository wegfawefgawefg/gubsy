#include "engine/menu/menu_system.hpp"

#include "engine/audio.hpp"
#include "engine/globals.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"
#include "game/state.hpp"
#include "game/ui_layout_ids.hpp"
#include "engine/input_binding_utils.hpp"
#include "engine/layout_editor/layout_editor_hooks.hpp"
#include "engine/layout_editor/layout_editor.hpp"
#include "engine/imgui_layer.hpp"

#include <vector>
#include <unordered_map>
#include <cmath>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>

namespace {

struct RuntimeCache {
    UILayoutId layout{kMenuIdInvalid};
    int width{0};
    int height{0};
    std::vector<MenuWidget> widgets;
    std::vector<SDL_FRect> rects;
};

RuntimeCache g_cache;
WidgetId g_focus{kMenuIdInvalid};
MenuInputState g_prev_input{};
MenuInputState g_current_input{};
bool g_active{false};
bool g_prev_mouse_down{false};
std::string* g_active_text_buffer{nullptr};
int g_active_text_max{0};
bool g_text_input_enabled{false};
SDL_FRect g_focus_rect{};
bool g_has_focus_rect{false};
SDL_Color g_focus_outline_color{120, 170, 255, 255};
bool g_has_focus_color{false};
bool g_text_edit_active{false};
WidgetId g_text_edit_widget{kMenuIdInvalid};
float g_caret_time{0.0f};
std::unordered_map<MenuScreenId, WidgetId> g_last_focus;
MenuScreenId g_current_screen{kMenuIdInvalid};
bool g_allow_mouse_focus{true};
bool g_mouse_focus_locked{false};
int g_mouse_focus_lock_x{0};
int g_mouse_focus_lock_y{0};

struct FocusArrowState {
    SDL_FPoint left_pos{0.0f, 0.0f};
    SDL_FPoint right_pos{0.0f, 0.0f};
    SDL_FPoint left_target{0.0f, 0.0f};
    SDL_FPoint right_target{0.0f, 0.0f};
    bool initialized{false};
    float time{0.0f};
} g_arrows;
void play_menu_sound(const char* key) {
    if (!key || !*key)
        return;
    play_sound(key);
}

void play_focus_sound() { play_menu_sound("base:ui_cursor_move"); }
void play_confirm_sound() { play_menu_sound("base:ui_confirm"); }
void play_cant_sound() { play_menu_sound("base:ui_cant"); }
void play_left_sound() { play_menu_sound("base:ui_left"); }
void play_right_sound() { play_menu_sound("base:ui_right"); }

void lock_mouse_focus_at(int x, int y);
void unlock_mouse_focus_now();

MenuWidget* find_widget(WidgetId id) {
    if (id == kMenuIdInvalid)
        return nullptr;
    for (auto& widget : g_cache.widgets) {
        if (widget.id == id)
            return &widget;
    }
    return nullptr;
}

MenuWidget* find_widget_by_slot(UILayoutObjectId slot) {
    if (slot == kMenuIdInvalid)
        return nullptr;
    for (auto& widget : g_cache.widgets) {
        if (widget.slot == slot)
            return &widget;
    }
    return nullptr;
}

WidgetId resolve_focus(WidgetId target) {
    if (target == kMenuIdInvalid)
        return kMenuIdInvalid;
    if (find_widget(target))
        return target;
    return kMenuIdInvalid;
}

WidgetId first_selectable_widget() {
    for (const auto& widget : g_cache.widgets) {
        if (widget.type != WidgetType::Label)
            return widget.id;
    }
    return kMenuIdInvalid;
}

SDL_FRect rect_from_object(const UIObject& obj, int width, int height) {
    SDL_FRect rect;
    rect.x = obj.x * static_cast<float>(width);
    rect.y = obj.y * static_cast<float>(height);
    rect.w = obj.w * static_cast<float>(width);
    rect.h = obj.h * static_cast<float>(height);
    return rect;
}

int measure_text_width(const char* text) {
    if (!text || !gg || !gg->ui_font)
        return 0;
    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(gg->ui_font, text, &w, &h) != 0)
        return 0;
    return w;
}

void draw_text_with_clip(SDL_Renderer* renderer,
                         const char* text,
                         int x,
                         int y,
                         SDL_Color color,
                         const SDL_Rect* clip) {
    if (!text)
        return;
    SDL_Rect prev_clip{};
    SDL_bool had_clip = SDL_RenderIsClipEnabled(renderer);
    if (had_clip)
        SDL_RenderGetClipRect(renderer, &prev_clip);
    if (clip)
        SDL_RenderSetClipRect(renderer, clip);
    draw_text(renderer, text, x, y, color);
    if (clip) {
        if (had_clip)
            SDL_RenderSetClipRect(renderer, &prev_clip);
        else
            SDL_RenderSetClipRect(renderer, nullptr);
    }
}

void begin_text_edit(MenuWidget& widget) {
    if (widget.type != WidgetType::TextInput || !widget.text_buffer)
        return;
    g_text_edit_active = true;
    g_text_edit_widget = widget.id;
    g_active_text_buffer = widget.text_buffer;
    g_active_text_max = widget.text_max_len;
    g_caret_time = 0.0f;
    if (!g_text_input_enabled) {
        SDL_StartTextInput();
        g_text_input_enabled = true;
    }
}

void end_text_edit() {
    if (!g_text_edit_active)
        return;
    g_text_edit_active = false;
    g_text_edit_widget = kMenuIdInvalid;
    g_active_text_buffer = nullptr;
    if (g_text_input_enabled) {
        SDL_StopTextInput();
        g_text_input_enabled = false;
    }
    if (es)
        lock_mouse_focus_at(es->device_state.mouse_x, es->device_state.mouse_y);
}


bool execute_action(const MenuAction& action, MenuContext& ctx, bool& stack_changed) {
    switch (action.type) {
        case MenuActionType::None:
            return false;
        case MenuActionType::PushScreen: {
            if (!ctx.manager.push_screen(static_cast<MenuScreenId>(action.a), ctx.player_index))
                return false;
            stack_changed = true;
            g_focus = kMenuIdInvalid;
            return true;
        }
        case MenuActionType::PopScreen:
            ctx.manager.pop_screen();
            stack_changed = true;
            g_focus = kMenuIdInvalid;
            end_text_edit();
            return true;
        case MenuActionType::RequestFocus: {
            WidgetId target = resolve_focus(static_cast<WidgetId>(action.a));
            if (target != kMenuIdInvalid) {
                g_focus = target;
                return true;
            }
            return false;
        }
        case MenuActionType::ToggleBool:
            if (action.ptr)
                *reinterpret_cast<bool*>(action.ptr) = !*reinterpret_cast<bool*>(action.ptr);
            return true;
        case MenuActionType::SetFloat:
            if (action.ptr)
                *reinterpret_cast<float*>(action.ptr) = action.f;
            return true;
        case MenuActionType::DeltaFloat:
            if (action.ptr)
                *reinterpret_cast<float*>(action.ptr) += action.f;
            return true;
        case MenuActionType::RunCommand:
            if (MenuCommandRegistry* registry = ctx.manager.commands())
                registry->invoke(ctx, static_cast<MenuCommandId>(action.a), action.b);
            return true;
        default:
            return false;
    }
}

void rebuild_cache(MenuManager::ScreenInstance& inst, MenuContext& ctx) {
    BuiltScreen built = inst.def->build(ctx);
    g_cache.layout = (built.layout != kMenuIdInvalid) ? built.layout : inst.def->layout;
    g_cache.width = ctx.screen_width;
    g_cache.height = ctx.screen_height;
    g_cache.widgets.assign(built.widgets.items.begin(), built.widgets.items.end());
    g_cache.rects.assign(g_cache.widgets.size(), SDL_FRect{});
    WidgetId remembered = kMenuIdInvalid;
    auto remembered_it = g_last_focus.find(g_current_screen);
    if (remembered_it != g_last_focus.end())
        remembered = remembered_it->second;
    if (g_focus == kMenuIdInvalid && remembered != kMenuIdInvalid)
        g_focus = remembered;
    if (g_focus == kMenuIdInvalid) {
        if (built.default_focus != kMenuIdInvalid)
            g_focus = built.default_focus;
        else
            g_focus = first_selectable_widget();
    }
    if (!find_widget(g_focus)) {
        if (built.default_focus != kMenuIdInvalid)
            g_focus = built.default_focus;
        if (!find_widget(g_focus))
            g_focus = first_selectable_widget();
    }
    MenuWidget* focus_widget = find_widget(g_focus);
    if (!focus_widget || focus_widget->type == WidgetType::Label) {
        if (built.default_focus != kMenuIdInvalid)
            g_focus = built.default_focus;
        focus_widget = find_widget(g_focus);
        if (!focus_widget || focus_widget->type == WidgetType::Label)
            g_focus = first_selectable_widget();
    }
    for (const MenuAction& act : built.frame_actions.items) {
        bool unused = false;
        execute_action(act, ctx, unused);
    }
}

void update_arrows(float dt) {
    if (!g_has_focus_rect) {
        g_arrows.initialized = false;
        return;
    }
    g_arrows.left_target = SDL_FPoint{g_focus_rect.x - 28.0f, g_focus_rect.y + g_focus_rect.h * 0.5f};
    g_arrows.right_target = SDL_FPoint{g_focus_rect.x + g_focus_rect.w + 28.0f,
                                       g_focus_rect.y + g_focus_rect.h * 0.5f};
    if (!g_arrows.initialized) {
        g_arrows.left_pos = g_arrows.left_target;
        g_arrows.right_pos = g_arrows.right_target;
        g_arrows.initialized = true;
    }
    float t = std::clamp(dt * 10.0f, 0.0f, 1.0f);
    auto blend = [t](float current, float target) {
        return current + (target - current) * t;
    };
    g_arrows.left_pos.x = blend(g_arrows.left_pos.x, g_arrows.left_target.x);
    g_arrows.left_pos.y = blend(g_arrows.left_pos.y, g_arrows.left_target.y);
    g_arrows.right_pos.x = blend(g_arrows.right_pos.x, g_arrows.right_target.x);
    g_arrows.right_pos.y = blend(g_arrows.right_pos.y, g_arrows.right_target.y);
    g_arrows.time += dt;
}

void draw_arrow(SDL_Renderer* renderer, const SDL_FPoint& tip, float dir, SDL_Color color) {
    float arrow_len = 18.0f;
    float wing = 7.0f;
    SDL_FPoint base{tip.x - dir * arrow_len, tip.y};
    SDL_FPoint wing_top{base.x, base.y - wing};
    SDL_FPoint wing_bottom{base.x, base.y + wing};

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLineF(renderer, tip.x, tip.y, wing_top.x, wing_top.y);
    SDL_RenderDrawLineF(renderer, tip.x, tip.y, wing_bottom.x, wing_bottom.y);
    SDL_RenderDrawLineF(renderer, wing_top.x, wing_top.y, base.x, base.y);
    SDL_RenderDrawLineF(renderer, wing_bottom.x, wing_bottom.y, base.x, base.y);
}

void draw_focus_arrows(SDL_Renderer* renderer) {
    if (!g_arrows.initialized || !g_has_focus_rect || !g_has_focus_color)
        return;
    float osc = std::sin(g_arrows.time * 6.0f) * 3.0f;
    SDL_FPoint left_tip{g_arrows.left_pos.x + osc, g_arrows.left_pos.y};
    SDL_FPoint right_tip{g_arrows.right_pos.x - osc, g_arrows.right_pos.y};
    draw_arrow(renderer, left_tip, 1.0f, g_focus_outline_color);
    draw_arrow(renderer, right_tip, -1.0f, g_focus_outline_color);
}

void lock_mouse_focus_at(int x, int y) {
    g_allow_mouse_focus = false;
    g_mouse_focus_locked = true;
    g_mouse_focus_lock_x = x;
    g_mouse_focus_lock_y = y;
}

void ensure_mouse_lock(int x, int y) {
    if (!g_allow_mouse_focus && !g_mouse_focus_locked)
        lock_mouse_focus_at(x, y);
}

void unlock_mouse_focus_if_moved(int x, int y) {
    if (!g_mouse_focus_locked)
        return;
    if (x != g_mouse_focus_lock_x || y != g_mouse_focus_lock_y) {
        g_mouse_focus_locked = false;
        g_allow_mouse_focus = true;
    }
}

void unlock_mouse_focus_now() {
    g_allow_mouse_focus = true;
    g_mouse_focus_locked = false;
}

} // namespace

void menu_system_set_input(const MenuInputState& input) {
    g_current_input = input;
}

void menu_system_update(float dt, int screen_width, int screen_height) {
    g_active = false;
    if (!es || !ss)
        return;

    MenuManager& manager = es->menu_manager;
    if (manager.stack().empty())
        return;

    g_active = true;
    if (g_text_edit_active)
        g_caret_time += dt;
    else
        g_caret_time = 0.0f;

    bool stack_changed = false;
    bool needs_rebuild = true;
    WidgetId prev_focus_frame = g_focus;

    g_has_focus_rect = false;
    g_has_focus_color = false;

    while (needs_rebuild && !manager.stack().empty()) {
        needs_rebuild = false;
        auto& inst = const_cast<MenuManager::ScreenInstance&>(manager.stack().back());
        g_current_screen = inst.def ? inst.def->id : kMenuIdInvalid;
        if (!inst.def || !inst.def->build)
            break;
        MenuContext ctx{*es,
                        *ss,
                        manager,
                        screen_width,
                        screen_height,
                        inst.player_index,
                        inst.def ? inst.def->id : kMenuIdInvalid,
                        inst.state_ptr};
        rebuild_cache(inst, ctx);
        MenuWidget* focus = find_widget(g_focus);
        MenuInputState prev = g_prev_input;
        g_prev_input = g_current_input;
        bool select_handled = false;

        const bool allow_mouse_input = !imgui_want_capture_mouse() && !layout_editor_is_active();
        int mouse_x = es->device_state.mouse_x;
        int mouse_y = es->device_state.mouse_y;
        Uint32 mouse_buttons = allow_mouse_input ? es->device_state.mouse_buttons : 0u;
        float render_mouse_x = static_cast<float>(mouse_x);
        float render_mouse_y = static_cast<float>(mouse_y);
        bool has_render_mouse = false;
        if (allow_mouse_input && g_cache.width > 0 && g_cache.height > 0) {
            if (mouse_render_position(es->device_state,
                                      static_cast<float>(g_cache.width),
                                      static_cast<float>(g_cache.height),
                                      render_mouse_x,
                                      render_mouse_y)) {
                has_render_mouse = true;
            }
        }
        bool mouse_down = allow_mouse_input &&
                          (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        bool mouse_clicked = allow_mouse_input && mouse_down && !g_prev_mouse_down;
        if (allow_mouse_input) {
            g_prev_mouse_down = mouse_down;
            ensure_mouse_lock(mouse_x, mouse_y);
            unlock_mouse_focus_if_moved(mouse_x, mouse_y);
            if (mouse_clicked)
                unlock_mouse_focus_now();
        } else {
            g_prev_mouse_down = false;
            has_render_mouse = false;
        }

        const UILayout* layout_rects = get_ui_layout_for_resolution(static_cast<int>(g_cache.layout),
                                                                   g_cache.width,
                                                                   g_cache.height);
        for (std::size_t idx = 0; idx < g_cache.widgets.size(); ++idx) {
            const MenuWidget& widget = g_cache.widgets[idx];
            const UIObject* obj = layout_rects ? get_ui_object(*layout_rects, static_cast<int>(widget.slot)) : nullptr;
            SDL_FRect rect;
            if (obj)
                rect = rect_from_object(*obj, g_cache.width, g_cache.height);
            else
                rect = SDL_FRect{
                    static_cast<float>(g_cache.width) * 0.3f,
                    static_cast<float>(g_cache.height) * 0.3f,
                    static_cast<float>(g_cache.width) * 0.4f,
                    60.0f};
            if (idx >= g_cache.rects.size())
                g_cache.rects.push_back(rect);
            else
                g_cache.rects[idx] = rect;

            if (widget.id == g_focus) {
                g_focus_rect = rect;
                g_has_focus_rect = true;
                g_focus_outline_color = SDL_Color{widget.style.focus_r,
                                                  widget.style.focus_g,
                                                  widget.style.focus_b,
                                                  widget.style.focus_a};
                g_has_focus_color = true;
            }
        }

        bool up_pressed = g_current_input.up && !prev.up;
        bool down_pressed = g_current_input.down && !prev.down;
        bool left_pressed = g_current_input.left && !prev.left;
        bool right_pressed = g_current_input.right && !prev.right;
        bool select_pressed = g_current_input.select && !prev.select;
        bool back_pressed = g_current_input.back && !prev.back;
        bool page_prev_pressed = g_current_input.page_prev && !prev.page_prev;
        bool page_next_pressed = g_current_input.page_next && !prev.page_next;

        bool editing_focus = g_text_edit_active && focus && focus->id == g_text_edit_widget;

        if (focus && focus->type == WidgetType::TextInput && select_pressed) {
            if (!editing_focus)
                begin_text_edit(*focus);
            else
                end_text_edit();
            select_handled = true;
            select_pressed = false;
            editing_focus = g_text_edit_active && focus && focus->id == g_text_edit_widget;
        }

        if (focus && editing_focus) {
            up_pressed = down_pressed = left_pressed = right_pressed = false;
            page_prev_pressed = page_next_pressed = false;
            if (back_pressed) {
                end_text_edit();
                back_pressed = false;
                editing_focus = false;
            }
        }

        if (focus && !editing_focus) {
            if (up_pressed) {
                lock_mouse_focus_at(mouse_x, mouse_y);
                if (focus->nav_up != kMenuIdInvalid) {
                    WidgetId target = resolve_focus(focus->nav_up);
                    if (target != kMenuIdInvalid)
                        g_focus = target;
                }
            } else if (down_pressed) {
                lock_mouse_focus_at(mouse_x, mouse_y);
                if (focus->nav_down != kMenuIdInvalid) {
                    WidgetId target = resolve_focus(focus->nav_down);
                    if (target != kMenuIdInvalid)
                        g_focus = target;
                }
            }

            if (!needs_rebuild && left_pressed) {
                lock_mouse_focus_at(mouse_x, mouse_y);
                bool handled = false;
                if (focus->on_left.type != MenuActionType::None) {
                    play_left_sound();
                    execute_action(focus->on_left, ctx, stack_changed);
                    needs_rebuild = true;
                    handled = true;
                    continue;
                } else if (focus->nav_left != kMenuIdInvalid) {
                    WidgetId target = resolve_focus(focus->nav_left);
                    if (target != kMenuIdInvalid) {
                        g_focus = target;
                        handled = true;
                    }
                }
                if (!handled)
                    play_cant_sound();
            }

            if (!needs_rebuild && right_pressed) {
                lock_mouse_focus_at(mouse_x, mouse_y);
                bool handled = false;
                if (focus->on_right.type != MenuActionType::None) {
                    play_right_sound();
                    execute_action(focus->on_right, ctx, stack_changed);
                    needs_rebuild = true;
                    handled = true;
                    continue;
                } else if (focus->nav_right != kMenuIdInvalid) {
                    WidgetId target = resolve_focus(focus->nav_right);
                    if (target != kMenuIdInvalid) {
                        g_focus = target;
                        handled = true;
                    }
                }
                if (!handled)
                    play_cant_sound();
            }

            if (!needs_rebuild) {
                if (select_pressed && focus->on_select.type != MenuActionType::None) {
                    lock_mouse_focus_at(mouse_x, mouse_y);
                    play_confirm_sound();
                    select_handled = true;
                    execute_action(focus->on_select, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                } else if (select_pressed && !select_handled) {
                    play_cant_sound();
                    select_pressed = false;
                } else if (back_pressed) {
                    if (focus->on_back.type != MenuActionType::None) {
                        lock_mouse_focus_at(mouse_x, mouse_y);
                        execute_action(focus->on_back, ctx, stack_changed);
                        needs_rebuild = true;
                        continue;
                    } else if (manager.stack().size() > 1) {
                        lock_mouse_focus_at(mouse_x, mouse_y);
                        MenuAction pop = MenuAction::pop();
                        execute_action(pop, ctx, stack_changed);
                        needs_rebuild = true;
                        continue;
                    } else {
                        back_pressed = false;
                    }
                }
            }

        if (!needs_rebuild && page_prev_pressed) {
            lock_mouse_focus_at(mouse_x, mouse_y);
            MenuAction action = MenuAction::none();
            if (MenuWidget* prev_widget = find_widget_by_slot(SettingsObjectID::PREV))
                action = prev_widget->on_select;
            if (action.type != MenuActionType::None) {
                play_left_sound();
                execute_action(action, ctx, stack_changed);
                needs_rebuild = true;
                continue;
            }
            play_cant_sound();
        }
        if (!needs_rebuild && page_next_pressed) {
            lock_mouse_focus_at(mouse_x, mouse_y);
            MenuAction action = MenuAction::none();
            if (MenuWidget* next_widget = find_widget_by_slot(SettingsObjectID::NEXT))
                action = next_widget->on_select;
            if (action.type != MenuActionType::None) {
                play_right_sound();
                execute_action(action, ctx, stack_changed);
                needs_rebuild = true;
                continue;
            }
            play_cant_sound();
        }
        }

        focus = find_widget(g_focus);
        if (!focus) {
            g_focus = first_selectable_widget();
            focus = find_widget(g_focus);
        }
        editing_focus = g_text_edit_active && focus && focus->id == g_text_edit_widget;
        if (g_text_edit_active && !editing_focus)
            end_text_edit();

        if (!focus) {
            g_focus = (!g_cache.widgets.empty()) ? g_cache.widgets.front().id : kMenuIdInvalid;
            focus = find_widget(g_focus);
        }

        if (allow_mouse_input) {
            WidgetId hovered = kMenuIdInvalid;
            for (std::size_t i = 0; i < g_cache.widgets.size() && i < g_cache.rects.size(); ++i) {
                const MenuWidget& widget = g_cache.widgets[i];
                if (widget.type == WidgetType::Label)
                    continue;
                const SDL_FRect& rect = g_cache.rects[i];
                float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                float fy = has_render_mouse ? render_mouse_y : static_cast<float>(mouse_y);
                bool inside = fx >= rect.x && fx <= rect.x + rect.w &&
                              fy >= rect.y && fy <= rect.y + rect.h;
                if (inside) {
                    hovered = widget.id;
                    break;
                }
            }
            bool hover_changed = hovered != g_focus;
            if (g_allow_mouse_focus && hover_changed &&
                hovered != kMenuIdInvalid && !mouse_down) {
                g_focus = hovered;
                focus = find_widget(g_focus);
            }
            if (hovered != kMenuIdInvalid && mouse_clicked) {
                g_focus = hovered;
                focus = find_widget(g_focus);
                bool click_handled = false;
                if (focus && focus->type == WidgetType::TextInput) {
                    begin_text_edit(*focus);
                    click_handled = true;
                } else {
                    end_text_edit();
                }
                if (focus && focus->on_select.type != MenuActionType::None) {
                    play_confirm_sound();
                    click_handled = true;
                    execute_action(focus->on_select, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                } else if (!click_handled) {
                    play_cant_sound();
                }
            }
        }

        if (g_current_screen != kMenuIdInvalid && focus)
            g_last_focus[g_current_screen] = focus->id;

    }

    update_arrows(dt);

    if (g_focus != prev_focus_frame && g_focus != kMenuIdInvalid)
        play_focus_sound();

    if (manager.stack().empty()) {
        g_current_screen = kMenuIdInvalid;
        end_text_edit();
        g_last_focus.clear();
        g_cache.widgets.clear();
        g_cache.layout = kMenuIdInvalid;
        g_focus = kMenuIdInvalid;
        g_cache.rects.clear();
        g_active_text_buffer = nullptr;
        g_arrows.initialized = false;
        g_has_focus_rect = false;
        g_has_focus_color = false;
        g_allow_mouse_focus = true;
        g_mouse_focus_locked = false;
    }
}

void menu_system_handle_text_input(const char* text) {
    if (!g_text_edit_active || !g_active_text_buffer || !text)
        return;
    while (*text) {
        if (g_active_text_max <= 0 || static_cast<int>(g_active_text_buffer->size()) < g_active_text_max)
            g_active_text_buffer->push_back(*text);
        ++text;
    }
}

void menu_system_handle_backspace() {
    if (!g_active_text_buffer) {
        if (g_focus != kMenuIdInvalid) {
            MenuWidget* focus_widget = find_widget(g_focus);
            if (focus_widget && focus_widget->type == WidgetType::TextInput)
                begin_text_edit(*focus_widget);
        }
    }
    if (!g_text_edit_active || !g_active_text_buffer)
        return;
    if (!g_active_text_buffer->empty())
        g_active_text_buffer->pop_back();
}

void menu_system_render(SDL_Renderer* renderer, int screen_width, int screen_height) {
    if ((g_cache.width != screen_width || g_cache.height != screen_height) && screen_width > 0 &&
        screen_height > 0) {
        MenuInputState zero_input{};
        g_prev_input = g_current_input;
        g_current_input = zero_input;
        menu_system_update(0.0f, screen_width, screen_height);
    }
    if (!renderer || g_cache.widgets.empty())
        return;

    const UILayout* layout = get_ui_layout_for_resolution(static_cast<int>(g_cache.layout),
                                                          g_cache.width,
                                                          g_cache.height);
    for (std::size_t i = 0; i < g_cache.widgets.size(); ++i) {
        const MenuWidget& widget = g_cache.widgets[i];
        SDL_FRect rect;
        if (i < g_cache.rects.size())
            rect = g_cache.rects[i];
        else {
            const UIObject* obj = layout ? get_ui_object(*layout, static_cast<int>(widget.slot)) : nullptr;
            if (obj)
                rect = rect_from_object(*obj, g_cache.width, g_cache.height);
            else
                rect = SDL_FRect{
                    static_cast<float>(g_cache.width) * 0.3f,
                    static_cast<float>(g_cache.height) * 0.3f,
                    static_cast<float>(g_cache.width) * 0.4f,
                    60.0f};
        }

        bool draw_background = widget.type != WidgetType::Label;
        if (draw_background) {
            SDL_Color bg{widget.style.bg_r, widget.style.bg_g, widget.style.bg_b, widget.style.bg_a};
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRectF(renderer, &rect);

            SDL_Color border{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, widget.style.fg_a};
            SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
            SDL_RenderDrawRectF(renderer, &rect);

            if (widget.id == g_focus) {
                SDL_Color focus{widget.style.focus_r, widget.style.focus_g, widget.style.focus_b, widget.style.focus_a};
                auto adjust = [](Uint8 base, int delta) -> Uint8 {
                    return static_cast<Uint8>(std::clamp(static_cast<int>(base) + delta, 0, 255));
                };
                int luma = static_cast<int>(0.2126f * widget.style.bg_r +
                                            0.7152f * widget.style.bg_g +
                                            0.0722f * widget.style.bg_b);
                int delta = (luma > 150) ? -18 : 22;
                SDL_Color focus_overlay{
                    adjust(widget.style.bg_r, delta),
                    adjust(widget.style.bg_g, delta),
                    adjust(widget.style.bg_b, delta),
                    70};
                SDL_SetRenderDrawColor(renderer, focus_overlay.r, focus_overlay.g, focus_overlay.b, focus_overlay.a);
                SDL_RenderFillRectF(renderer, &rect);

                SDL_FRect outline = rect;
                outline.x -= 2.0f;
                outline.y -= 2.0f;
                outline.w += 4.0f;
                outline.h += 4.0f;
                SDL_SetRenderDrawColor(renderer, focus.r, focus.g, focus.b, focus.a);
                SDL_RenderDrawRectF(renderer, &outline);

                SDL_FRect inner = rect;
                inner.x += 1.0f;
                inner.y += 1.0f;
                inner.w -= 2.0f;
                inner.h -= 2.0f;
                SDL_Color inner_col{
                    adjust(focus.r, 10),
                    adjust(focus.g, 10),
                    adjust(focus.b, 10),
                    focus.a};
                SDL_SetRenderDrawColor(renderer, inner_col.r, inner_col.g, inner_col.b, inner_col.a);
                SDL_RenderDrawRectF(renderer, &inner);
            }
        }

        std::string text_storage;
        const char* text_ptr = widget.label;
        SDL_Color text_color{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255};
        if (widget.type == WidgetType::TextInput && widget.text_buffer) {
            if (widget.text_buffer->empty() && widget.placeholder) {
                text_ptr = widget.placeholder;
                text_color = SDL_Color{160, 160, 180, 255};
            } else {
                text_storage = *widget.text_buffer;
                text_ptr = text_storage.c_str();
            }
        }

        bool is_label = widget.type == WidgetType::Label;
        SDL_Rect clip{
            static_cast<int>(rect.x) + 8,
            static_cast<int>(rect.y) + 4,
            std::max(0, static_cast<int>(rect.w) - 16),
            std::max(0, static_cast<int>(rect.h) - 8)};
        const SDL_Rect* clip_ptr = is_label ? nullptr : &clip;
        int line_x = static_cast<int>(rect.x) + (is_label ? 0 : 16);
        int line_y = static_cast<int>(rect.y) + (is_label ? 0 : 6);
        auto next_line = [&]() {
            line_y += 22;
        };
        if (text_ptr) {
            draw_text_with_clip(renderer, text_ptr, line_x, line_y, text_color, clip_ptr);
            next_line();
        }
        if (widget.secondary) {
            SDL_Color sec_color{static_cast<Uint8>(widget.style.fg_r / 2 + 50),
                                static_cast<Uint8>(widget.style.fg_g / 2 + 50),
                                static_cast<Uint8>(widget.style.fg_b / 2 + 50),
                                255};
            draw_text_with_clip(renderer, widget.secondary, line_x, line_y, sec_color, clip_ptr);
            next_line();
        }
        if (widget.tertiary) {
            SDL_Color tert_color{static_cast<Uint8>(widget.style.fg_r / 3 + 60),
                                 static_cast<Uint8>(widget.style.fg_g / 3 + 60),
                                 static_cast<Uint8>(widget.style.fg_b / 3 + 60),
                                 255};
            draw_text_with_clip(renderer, widget.tertiary, line_x, line_y, tert_color, clip_ptr);
        }
        if (widget.type == WidgetType::TextInput &&
            g_text_edit_active && widget.id == g_text_edit_widget && widget.text_buffer) {
            if (std::fmod(g_caret_time, 1.0f) < 0.5f) {
                int caret_x = line_x + measure_text_width(widget.text_buffer->c_str());
                int caret_top = line_y - 2;
                int caret_bottom = caret_top + 18;
                SDL_SetRenderDrawColor(renderer, widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255);
                SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
            }
        }
        if (widget.badge) {
            int badge_w = measure_text_width(widget.badge);
            int badge_x = static_cast<int>(rect.x + rect.w) - badge_w - 12;
            if (badge_x < static_cast<int>(rect.x) + 8)
                badge_x = static_cast<int>(rect.x) + 8;
            int badge_y = static_cast<int>(rect.y) + 6;
            SDL_Rect badge_clip{
                static_cast<int>(rect.x) + 8,
                static_cast<int>(rect.y) + 4,
                std::max(0, static_cast<int>(rect.w) - 16),
                std::max(0, static_cast<int>(rect.h) - 8)};
            draw_text_with_clip(renderer, widget.badge, badge_x, badge_y, widget.badge_color, &badge_clip);
        }
    }

    draw_focus_arrows(renderer);
}

void menu_system_reset() {
    end_text_edit();
    g_last_focus.clear();
    g_cache.widgets.clear();
    g_cache.layout = kMenuIdInvalid;
    g_cache.rects.clear();
    g_focus = kMenuIdInvalid;
    g_prev_input = {};
    g_current_input = {};
    g_active_text_buffer = nullptr;
    if (g_text_input_enabled) {
        SDL_StopTextInput();
        g_text_input_enabled = false;
    }
    g_allow_mouse_focus = true;
    g_mouse_focus_locked = false;
    g_mouse_focus_lock_x = 0;
    g_mouse_focus_lock_y = 0;
}

bool menu_system_active() {
    return g_active;
}
