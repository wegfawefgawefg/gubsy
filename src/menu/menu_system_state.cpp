#include "src/menu/menu_system_state.hpp"

#include "src/audio.hpp"
#include "src/engine_state.hpp"
#include "src/graphics.hpp"
#include "src/render.hpp"
#include "src/ui_layouts.hpp"

#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace menu_system_internal {

MenuRuntimeState& runtime_state(EngineState& engine) {
    return engine.menu_runtime;
}

const MenuRuntimeState& runtime_state(const EngineState& engine) {
    return engine.menu_runtime;
}

namespace {

void play_menu_sound(EngineState& engine, const char* key) {
    if (!key || !*key)
        return;
    play_sound(engine, key);
}

} // namespace

void play_focus_sound(EngineState& engine) {
    play_menu_sound(engine, "base:ui_cursor_move");
}
void play_confirm_sound(EngineState& engine) {
    play_menu_sound(engine, "base:ui_confirm");
}
void play_cant_sound(EngineState& engine) {
    play_menu_sound(engine, "base:ui_cant");
}
void play_left_sound(EngineState& engine) {
    play_menu_sound(engine, "base:ui_left");
}
void play_right_sound(EngineState& engine) {
    play_menu_sound(engine, "base:ui_right");
}

void lock_mouse_focus_at(MenuRuntimeState& state, int x, int y) {
    state.allow_mouse_focus = false;
    state.mouse_focus_locked = true;
    state.mouse_focus_lock_x = x;
    state.mouse_focus_lock_y = y;
}

void ensure_mouse_lock(MenuRuntimeState& state, int x, int y) {
    if (!state.allow_mouse_focus && !state.mouse_focus_locked)
        lock_mouse_focus_at(state, x, y);
}

void unlock_mouse_focus_if_moved(MenuRuntimeState& state, int x, int y) {
    if (!state.mouse_focus_locked)
        return;
    if (x != state.mouse_focus_lock_x || y != state.mouse_focus_lock_y) {
        state.mouse_focus_locked = false;
        state.allow_mouse_focus = true;
    }
}

void unlock_mouse_focus_now(MenuRuntimeState& state) {
    state.allow_mouse_focus = true;
    state.mouse_focus_locked = false;
}

MenuWidget* find_widget(MenuRuntimeState& state, WidgetId id) {
    if (id == kMenuIdInvalid)
        return nullptr;
    for (auto& widget : state.cache.widgets) {
        if (widget.id == id)
            return &widget;
    }
    return nullptr;
}

const MenuWidget* find_widget(const MenuRuntimeState& state, WidgetId id) {
    return find_widget(const_cast<MenuRuntimeState&>(state), id);
}

MenuWidget* find_widget_by_slot(MenuRuntimeState& state, UILayoutObjectId slot) {
    if (slot == kMenuIdInvalid)
        return nullptr;
    for (auto& widget : state.cache.widgets) {
        if (widget.slot == slot)
            return &widget;
    }
    return nullptr;
}

SDL_FRect* find_widget_rect(MenuRuntimeState& state, WidgetId id) {
    for (std::size_t i = 0; i < state.cache.widgets.size() && i < state.cache.rects.size(); ++i) {
        if (state.cache.widgets[i].id == id)
            return &state.cache.rects[i];
    }
    return nullptr;
}

bool is_transient_focus_slot(UILayoutObjectId slot) {
    switch (slot) {
    case SettingsObjectID::BACK:
        return true;
    default:
        return false;
    }
}

SliderLayout compute_slider_layout(const MenuWidget& widget, const SDL_FRect& rect) {
    SliderLayout layout;
    layout.has_input = widget.text_buffer && widget.text_max_len > 0;
    if (layout.has_input) {
        layout.input_rect.w = std::min(rect.w * 0.28f, 120.0f);
        layout.input_rect.h = 24.0f;
        layout.input_rect.x = rect.x + rect.w - layout.input_rect.w - 12.0f;
        layout.input_rect.y = rect.y + rect.h * 0.2f;
    }
    float arrow_reserved = 0.0f;
    if (widget.has_discrete_options) {
        layout.has_buttons = true;
        float btn_width = kSliderOptionButtonWidth;
        float btn_height = std::min(rect.h * 0.35f, 24.0f);
        float spacing = kSliderOptionButtonSpacing;
        float base_y = rect.y + rect.h - btn_height - 8.0f;
        float right_x = rect.x + rect.w - btn_width - 12.0f;
        layout.right_btn = SDL_FRect{right_x, base_y, btn_width, btn_height};
        layout.left_btn = SDL_FRect{right_x - (btn_width + spacing), base_y, btn_width, btn_height};
        arrow_reserved = (btn_width * 2.0f) + spacing + 8.0f;
    }
    if (layout.has_input) {
        layout.input_rect.x -= arrow_reserved;
        if (layout.input_rect.x < rect.x + 12.0f)
            layout.input_rect.x = rect.x + 12.0f;
    }
    float reserved_right =
        (layout.has_input ? (layout.input_rect.w + 18.0f) : rect.w * 0.1f) + arrow_reserved;
    layout.track_left = rect.x + rect.w * 0.08f;
    layout.track_right = rect.x + rect.w - reserved_right;
    layout.track_y = rect.y + rect.h - 14.0f;
    return layout;
}

OptionLayout compute_option_layout(const MenuWidget& widget, const SDL_FRect& rect) {
    OptionLayout layout;
    float btn_width = std::min(rect.w * 0.12f, 44.0f);
    float btn_height = std::min(rect.h * 0.4f, 24.0f);
    float spacing = 6.0f;
    float base_y = rect.y + rect.h - btn_height - 12.0f;
    float right_btn_x = rect.x + rect.w - btn_width - 12.0f;
    float left_btn_x = right_btn_x - btn_width - spacing;
    layout.left_btn = SDL_FRect{left_btn_x, base_y, btn_width, btn_height};
    layout.right_btn = SDL_FRect{right_btn_x, base_y, btn_width, btn_height};
    float label_left = rect.x + 16.0f;
    float label_right = left_btn_x - spacing;
    float label_width = std::max(20.0f, label_right - label_left);
    float value_height = std::min(26.0f, rect.h * 0.35f);
    float value_y = rect.y + rect.h * 0.8f;
    float max_y = base_y - value_height - rect.h * 0.04f;
    if (value_y > max_y)
        value_y = max_y;
    float min_y = rect.y + rect.h * 0.55f;
    if (value_y < min_y)
        value_y = min_y;
    layout.value_rect = SDL_FRect{label_left, value_y, label_width, value_height};
    if (widget.text_buffer && widget.text_max_len > 0) {
        layout.has_primary_input = true;
        float input_width = std::min(rect.w * 0.2f, 110.0f);
        float input_height = 24.0f;
        float input_y = rect.y + rect.h * 0.18f;
        layout.primary_input =
            SDL_FRect{rect.x + rect.w - input_width - 12.0f, input_y, input_width, input_height};
        if (widget.aux_text_buffer && widget.aux_text_max_len > 0) {
            layout.has_secondary_input = true;
            layout.primary_input.x -= (input_width + 10.0f);
            layout.secondary_input = SDL_FRect{rect.x + rect.w - input_width - 12.0f, input_y,
                                               input_width, input_height};
        }
    }
    return layout;
}

bool point_in_rect(float x, float y, const SDL_FRect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

WidgetId resolve_focus(const MenuRuntimeState& state, WidgetId target) {
    if (target == kMenuIdInvalid)
        return kMenuIdInvalid;
    if (find_widget(state, target))
        return target;
    return kMenuIdInvalid;
}

WidgetId first_selectable_widget(const MenuRuntimeState& state) {
    for (const auto& widget : state.cache.widgets) {
        if (widget.type != WidgetType::Label)
            return widget.id;
    }
    return kMenuIdInvalid;
}

void reset_repeat_state(NavRepeatState& state) {
    state.active = false;
    state.repeated = false;
    state.timer = 0.0f;
}

void update_repeat(bool down, NavRepeatState& state, bool& trigger, float dt) {
    if (!down) {
        reset_repeat_state(state);
        return;
    }
    if (!state.active) {
        state.active = true;
        state.timer = 0.0f;
        state.repeated = false;
        return;
    }
    state.timer += dt;
    float threshold = state.repeated ? kRepeatInterval : kRepeatDelay;
    if (state.timer >= threshold) {
        trigger = true;
        state.timer = 0.0f;
        state.repeated = true;
    }
}

SDL_FRect rect_from_object(const UIObject& obj, int width, int height) {
    SDL_FRect rect;
    rect.x = obj.rect.x * static_cast<float>(width);
    rect.y = obj.rect.y * static_cast<float>(height);
    rect.w = obj.rect.w * static_cast<float>(width);
    rect.h = obj.rect.h * static_cast<float>(height);
    return rect;
}

int measure_text_width(const EngineState& engine, const char* text) {
    const Graphics* graphics = current_graphics(engine);
    if (!text || !graphics || !graphics->ui_font)
        return 0;
    int w = 0;
    int h = 0;
    if (!TTF_GetStringSize(graphics->ui_font, text, 0, &w, &h))
        return 0;
    return w;
}

void draw_text_with_clip(const EngineState&, SDL_Renderer* renderer, const char* text, int x, int y,
                         SDL_Color color, const SDL_Rect* clip) {
    if (!text)
        return;
    SDL_Rect prev_clip{};
    bool had_clip = SDL_RenderClipEnabled(renderer);
    if (had_clip)
        SDL_GetRenderClipRect(renderer, &prev_clip);
    if (clip)
        SDL_SetRenderClipRect(renderer, clip);
    draw_text(renderer, text, x, y, color);
    if (clip) {
        if (had_clip)
            SDL_SetRenderClipRect(renderer, &prev_clip);
        else
            SDL_SetRenderClipRect(renderer, nullptr);
    }
}

void begin_text_edit(MenuRuntimeState& state, MenuWidget& widget, bool use_aux_buffer) {
    std::string* target_buffer = widget.text_buffer;
    int target_len = widget.text_max_len;
    if (use_aux_buffer && widget.aux_text_buffer) {
        target_buffer = widget.aux_text_buffer;
        target_len = widget.aux_text_max_len;
    }
    if (!target_buffer || target_len <= 0)
        return;
    state.text_edit_active = true;
    state.text_edit_widget = widget.id;
    state.active_text_buffer = target_buffer;
    state.active_text_max = target_len;
    state.text_edit_using_aux = use_aux_buffer;
    state.caret_time = 0.0f;
    if (!state.text_input_enabled) {
        SDL_StartTextInput(SDL_GetKeyboardFocus());
        state.text_input_enabled = true;
    }
}

bool commit_text_edit(MenuRuntimeState& state) {
    if (!state.text_edit_active || !state.active_text_buffer)
        return false;
    MenuWidget* widget = find_widget(state, state.text_edit_widget);
    bool modified = true;
    if (!widget || !widget->bind_ptr)
        return modified;
    if (widget->type == WidgetType::Slider1D) {
        char* end_ptr = nullptr;
        float parsed = std::strtof(state.active_text_buffer->c_str(), &end_ptr);
        bool parsed_ok = (end_ptr != state.active_text_buffer->c_str());
        if (!parsed_ok) {
            std::string lower = *state.active_text_buffer;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower == "unlimited") {
                parsed = 0.0f;
                parsed_ok = true;
            }
        }
        if (parsed_ok) {
            parsed = std::clamp(parsed, widget->min, widget->max);
            float* target = reinterpret_cast<float*>(widget->bind_ptr);
            if (*target != parsed) {
                *target = parsed;
                modified = true;
            } else {
                modified = false;
            }
        } else {
            modified = false;
        }
    }
    return modified;
}

bool end_text_edit(MenuRuntimeState& state) {
    if (!state.text_edit_active)
        return false;
    bool modified = commit_text_edit(state);
    state.text_edit_active = false;
    state.text_edit_widget = kMenuIdInvalid;
    state.active_text_buffer = nullptr;
    state.active_text_max = 0;
    state.text_edit_using_aux = false;
    if (state.text_input_enabled) {
        SDL_StopTextInput(SDL_GetKeyboardFocus());
        state.text_input_enabled = false;
    }
    lock_mouse_focus_at(state, state.last_mouse_x, state.last_mouse_y);
    return modified;
}

bool is_text_edit_widget(const MenuRuntimeState& state, WidgetId id) {
    return state.text_edit_active && state.text_edit_widget == id;
}

void set_active_text_buffer(MenuRuntimeState& state, std::string* buffer, int max_len) {
    if (!state.text_edit_active)
        return;
    state.active_text_buffer = buffer;
    state.active_text_max = max_len;
}

WidgetId current_text_widget(const MenuRuntimeState& state) {
    return state.text_edit_active ? state.text_edit_widget : kMenuIdInvalid;
}

bool execute_action(MenuRuntimeState& state, const MenuAction& action, MenuContext& ctx,
                    bool& stack_changed) {
    switch (action.type) {
    case MenuActionType::None:
        return false;
    case MenuActionType::PushScreen: {
        if (!ctx.manager.push_screen(static_cast<MenuScreenId>(action.a), ctx.player_index))
            return false;
        stack_changed = true;
        state.focus = kMenuIdInvalid;
        return true;
    }
    case MenuActionType::PopScreen:
        if (ctx.manager.stack().size() <= 1)
            return false;
        ctx.manager.pop_screen();
        stack_changed = true;
        state.focus = kMenuIdInvalid;
        end_text_edit(state);
        return true;
    case MenuActionType::RequestFocus: {
        WidgetId target = resolve_focus(state, static_cast<WidgetId>(action.a));
        if (target != kMenuIdInvalid) {
            state.focus = target;
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

void rebuild_cache(MenuRuntimeState& state, MenuManager::ScreenInstance& inst, MenuContext& ctx) {
    BuiltScreen built = inst.def->build(ctx);
    state.cache.layout = (built.layout != kMenuIdInvalid) ? built.layout : inst.def->layout;
    state.cache.width = ctx.screen_width;
    state.cache.height = ctx.screen_height;
    state.cache.widgets.assign(built.widgets.items.begin(), built.widgets.items.end());
    state.cache.rects.assign(state.cache.widgets.size(), SDL_FRect{});
    WidgetId remembered = kMenuIdInvalid;
    auto remembered_it = state.last_focus.find(state.current_screen);
    if (remembered_it != state.last_focus.end())
        remembered = remembered_it->second;
    if (remembered != kMenuIdInvalid) {
        if (MenuWidget* remembered_widget = find_widget(state, remembered)) {
            if (is_transient_focus_slot(remembered_widget->slot))
                remembered = kMenuIdInvalid;
        }
    }
    if (state.focus == kMenuIdInvalid && remembered != kMenuIdInvalid)
        state.focus = remembered;
    if (state.focus == kMenuIdInvalid) {
        if (built.default_focus != kMenuIdInvalid)
            state.focus = built.default_focus;
        else
            state.focus = first_selectable_widget(state);
    }
    if (!find_widget(state, state.focus)) {
        if (built.default_focus != kMenuIdInvalid)
            state.focus = built.default_focus;
        if (!find_widget(state, state.focus))
            state.focus = first_selectable_widget(state);
    }
    MenuWidget* focus_widget = find_widget(state, state.focus);
    if (!focus_widget || focus_widget->type == WidgetType::Label) {
        if (built.default_focus != kMenuIdInvalid)
            state.focus = built.default_focus;
        focus_widget = find_widget(state, state.focus);
        if (!focus_widget || focus_widget->type == WidgetType::Label)
            state.focus = first_selectable_widget(state);
    }
    for (const MenuAction& act : built.frame_actions.items) {
        bool unused = false;
        execute_action(state, act, ctx, unused);
    }
}

void update_arrows(MenuRuntimeState& state, float dt) {
    if (!state.has_focus_rect) {
        state.arrows.initialized = false;
        return;
    }
    state.arrows.left_target =
        SDL_FPoint{state.focus_rect.x - 28.0f, state.focus_rect.y + state.focus_rect.h * 0.5f};
    state.arrows.right_target = SDL_FPoint{state.focus_rect.x + state.focus_rect.w + 28.0f,
                                           state.focus_rect.y + state.focus_rect.h * 0.5f};
    if (!state.arrows.initialized) {
        state.arrows.left_pos = state.arrows.left_target;
        state.arrows.right_pos = state.arrows.right_target;
        state.arrows.initialized = true;
    }
    float t = std::clamp(dt * 40.0f, 0.0f, 1.0f);
    auto blend = [t](float current, float target) { return current + (target - current) * t; };
    state.arrows.left_pos.x = blend(state.arrows.left_pos.x, state.arrows.left_target.x);
    state.arrows.left_pos.y = blend(state.arrows.left_pos.y, state.arrows.left_target.y);
    state.arrows.right_pos.x = blend(state.arrows.right_pos.x, state.arrows.right_target.x);
    state.arrows.right_pos.y = blend(state.arrows.right_pos.y, state.arrows.right_target.y);
    state.arrows.time += dt;
}

namespace {

void draw_arrow(SDL_Renderer* renderer, const SDL_FPoint& tip, float dir, SDL_Color color) {
    float arrow_len = 18.0f;
    float wing = 7.0f;
    SDL_FPoint base{tip.x - dir * arrow_len, tip.y};
    SDL_FPoint wing_top{base.x, base.y - wing};
    SDL_FPoint wing_bottom{base.x, base.y + wing};

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderLine(renderer, tip.x, tip.y, wing_top.x, wing_top.y);
    SDL_RenderLine(renderer, tip.x, tip.y, wing_bottom.x, wing_bottom.y);
    SDL_RenderLine(renderer, wing_top.x, wing_top.y, base.x, base.y);
    SDL_RenderLine(renderer, wing_bottom.x, wing_bottom.y, base.x, base.y);
}

} // namespace

void draw_focus_arrows(const MenuRuntimeState& state, SDL_Renderer* renderer) {
    if (!state.arrows.initialized || !state.has_focus_rect || !state.has_focus_color)
        return;
    float osc = std::sin(state.arrows.time * 6.0f) * 3.0f;
    SDL_FPoint left_tip{state.arrows.left_pos.x + osc, state.arrows.left_pos.y};
    SDL_FPoint right_tip{state.arrows.right_pos.x - osc, state.arrows.right_pos.y};
    draw_arrow(renderer, left_tip, 1.0f, state.focus_outline_color);
    draw_arrow(renderer, right_tip, -1.0f, state.focus_outline_color);
}

} // namespace menu_system_internal
