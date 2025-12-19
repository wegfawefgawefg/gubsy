#include "engine/menu/menu_system.hpp"

#include "engine/globals.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"
#include "game/state.hpp"

#include <vector>
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

MenuWidget* find_widget(WidgetId id) {
    if (id == kMenuIdInvalid)
        return nullptr;
    for (auto& widget : g_cache.widgets) {
        if (widget.id == id)
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

bool execute_action(const MenuAction& action, MenuContext& ctx, bool& stack_changed) {
    switch (action.type) {
        case MenuActionType::None:
            return false;
        case MenuActionType::PushScreen:
            ctx.manager.push_screen(static_cast<MenuScreenId>(action.a));
            stack_changed = true;
            g_focus = kMenuIdInvalid;
            return true;
        case MenuActionType::PopScreen:
            ctx.manager.pop_screen();
            stack_changed = true;
            g_focus = kMenuIdInvalid;
            return true;
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
    if (g_focus == kMenuIdInvalid) {
        if (built.default_focus != kMenuIdInvalid)
            g_focus = built.default_focus;
        else if (!g_cache.widgets.empty())
            g_focus = g_cache.widgets.front().id;
    }
    for (const MenuAction& act : built.frame_actions.items) {
        bool unused = false;
        execute_action(act, ctx, unused);
    }
}

} // namespace

void menu_system_set_input(const MenuInputState& input) {
    g_current_input = input;
}

void menu_system_update(float dt, int screen_width, int screen_height) {
    (void)dt;
    g_active = false;
    if (!es || !ss)
        return;

    MenuManager& manager = es->menu_manager;
    if (manager.stack().empty())
        return;

    g_active = true;

    bool stack_changed = false;
    bool needs_rebuild = true;

    while (needs_rebuild && !manager.stack().empty()) {
        needs_rebuild = false;
        auto& inst = const_cast<MenuManager::ScreenInstance&>(manager.stack().back());
        if (!inst.def || !inst.def->build)
            break;
        MenuContext ctx{*es, *ss, manager, screen_width, screen_height, inst.state_ptr};
        rebuild_cache(inst, ctx);

        MenuWidget* focus = find_widget(g_focus);
        MenuInputState prev = g_prev_input;
        g_prev_input = g_current_input;

        bool mouse_down = (es->device_state.mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        bool mouse_clicked = mouse_down && !g_prev_mouse_down;
        g_prev_mouse_down = mouse_down;
        int mouse_x = es->device_state.mouse_x;
        int mouse_y = es->device_state.mouse_y;

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
        }

        bool up_pressed = g_current_input.up && !prev.up;
        bool down_pressed = g_current_input.down && !prev.down;
        bool left_pressed = g_current_input.left && !prev.left;
        bool right_pressed = g_current_input.right && !prev.right;
        bool select_pressed = g_current_input.select && !prev.select;
        bool back_pressed = g_current_input.back && !prev.back;
        bool page_prev_pressed = g_current_input.page_prev && !prev.page_prev;
        bool page_next_pressed = g_current_input.page_next && !prev.page_next;

        if (focus) {
            if (up_pressed) {
                if (focus->nav_up != kMenuIdInvalid)
                    g_focus = resolve_focus(focus->nav_up);
            } else if (down_pressed) {
                if (focus->nav_down != kMenuIdInvalid)
                    g_focus = resolve_focus(focus->nav_down);
            }

            if (!needs_rebuild && left_pressed) {
                if (focus->nav_left != kMenuIdInvalid) {
                    g_focus = resolve_focus(focus->nav_left);
                } else if (focus->on_left.type != MenuActionType::None) {
                    execute_action(focus->on_left, ctx, stack_changed);
                    needs_rebuild = true;
                }
            }

            if (!needs_rebuild && right_pressed) {
                if (focus->nav_right != kMenuIdInvalid) {
                    g_focus = resolve_focus(focus->nav_right);
                } else if (focus->on_right.type != MenuActionType::None) {
                    execute_action(focus->on_right, ctx, stack_changed);
                    needs_rebuild = true;
                }
            }

            if (!needs_rebuild) {
                if (select_pressed && focus->on_select.type != MenuActionType::None) {
                    execute_action(focus->on_select, ctx, stack_changed);
                    needs_rebuild = true;
                } else if (back_pressed) {
                    if (focus->on_back.type != MenuActionType::None) {
                        execute_action(focus->on_back, ctx, stack_changed);
                        needs_rebuild = true;
                    } else {
                        MenuAction pop = MenuAction::pop();
                        execute_action(pop, ctx, stack_changed);
                        needs_rebuild = true;
                    }
                }
            }

            if (!needs_rebuild && page_prev_pressed && focus->on_left.type != MenuActionType::None) {
                execute_action(focus->on_left, ctx, stack_changed);
                needs_rebuild = true;
            }
            if (!needs_rebuild && page_next_pressed && focus->on_right.type != MenuActionType::None) {
                execute_action(focus->on_right, ctx, stack_changed);
                needs_rebuild = true;
            }
        } else {
            g_focus = (!g_cache.widgets.empty()) ? g_cache.widgets.front().id : kMenuIdInvalid;
            focus = find_widget(g_focus);
        }

        // Mouse hover and click
        WidgetId hovered = kMenuIdInvalid;
        for (std::size_t i = 0; i < g_cache.widgets.size() && i < g_cache.rects.size(); ++i) {
            const MenuWidget& widget = g_cache.widgets[i];
            if (widget.type == WidgetType::Label)
                continue;
            const SDL_FRect& rect = g_cache.rects[i];
            float fx = static_cast<float>(mouse_x);
            float fy = static_cast<float>(mouse_y);
            bool inside = fx >= rect.x && fx <= rect.x + rect.w &&
                          fy >= rect.y && fy <= rect.y + rect.h;
            if (inside) {
                hovered = widget.id;
                break;
            }
        }
        if (hovered != kMenuIdInvalid && hovered != g_focus && !mouse_down) {
            g_focus = hovered;
            focus = find_widget(g_focus);
        }
        if (hovered != kMenuIdInvalid && mouse_clicked) {
            g_focus = hovered;
            focus = find_widget(g_focus);
            if (focus && focus->on_select.type != MenuActionType::None) {
                execute_action(focus->on_select, ctx, stack_changed);
                needs_rebuild = true;
            }
        }

        bool wants_text_input = focus && focus->type == WidgetType::TextInput && focus->text_buffer;
        if (wants_text_input) {
            g_active_text_buffer = focus->text_buffer;
            g_active_text_max = focus->text_max_len;
            if (!g_text_input_enabled) {
                SDL_StartTextInput();
                g_text_input_enabled = true;
            }
        } else {
            g_active_text_buffer = nullptr;
            if (g_text_input_enabled) {
                SDL_StopTextInput();
                g_text_input_enabled = false;
            }
        }
    }

    if (manager.stack().empty()) {
        g_cache.widgets.clear();
        g_cache.layout = kMenuIdInvalid;
        g_focus = kMenuIdInvalid;
        g_cache.rects.clear();
        g_active_text_buffer = nullptr;
        if (g_text_input_enabled) {
            SDL_StopTextInput();
            g_text_input_enabled = false;
        }
    }
}

void menu_system_handle_text_input(const char* text) {
    if (!g_active_text_buffer || !text)
        return;
    while (*text) {
        if (g_active_text_max <= 0 || static_cast<int>(g_active_text_buffer->size()) < g_active_text_max)
            g_active_text_buffer->push_back(*text);
        ++text;
    }
}

void menu_system_handle_backspace() {
    if (!g_active_text_buffer)
        return;
    if (!g_active_text_buffer->empty())
        g_active_text_buffer->pop_back();
}

void menu_system_render(SDL_Renderer* renderer) {
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
                SDL_FRect outline = rect;
                outline.x -= 2.0f;
                outline.y -= 2.0f;
                outline.w += 4.0f;
                outline.h += 4.0f;
                SDL_SetRenderDrawColor(renderer, focus.r, focus.g, focus.b, focus.a);
                SDL_RenderDrawRectF(renderer, &outline);
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
}

void menu_system_reset() {
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
}

bool menu_system_active() {
    return g_active;
}
