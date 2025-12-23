#include "engine/menu/menu_system_state.hpp"

#include "engine/audio.hpp"
#include "engine/globals.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"

#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace menu_system_internal {

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

NavRepeatState g_repeat_up{};
NavRepeatState g_repeat_down{};
NavRepeatState g_repeat_left{};
NavRepeatState g_repeat_right{};

WidgetId g_slider_drag_id{kMenuIdInvalid};
FocusArrowState g_arrows;

namespace {

void play_menu_sound(const char* key) {
    if (!key || !*key)
        return;
    play_sound(key);
}

} // namespace

void play_focus_sound() { play_menu_sound("base:ui_cursor_move"); }
void play_confirm_sound() { play_menu_sound("base:ui_confirm"); }
void play_cant_sound() { play_menu_sound("base:ui_cant"); }
void play_left_sound() { play_menu_sound("base:ui_left"); }
void play_right_sound() { play_menu_sound("base:ui_right"); }

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

SDL_FRect* find_widget_rect(WidgetId id) {
    for (std::size_t i = 0; i < g_cache.widgets.size() && i < g_cache.rects.size(); ++i) {
        if (g_cache.widgets[i].id == id)
            return &g_cache.rects[i];
    }
    return nullptr;
}

bool is_transient_focus_slot(UILayoutObjectId slot) {
    switch (slot) {
        case SettingsObjectID::BACK:
        case SettingsObjectID::PREV:
        case SettingsObjectID::NEXT:
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
    float reserved_right = layout.has_input ? (layout.input_rect.w + 18.0f) : rect.w * 0.1f;
    layout.track_left = rect.x + rect.w * 0.08f;
    layout.track_right = rect.x + rect.w - reserved_right;
    layout.track_y = rect.y + rect.h - 14.0f;
    return layout;
}

OptionLayout compute_option_layout(const SDL_FRect& rect) {
    OptionLayout layout;
    float btn_width = std::min(rect.w * 0.14f, 48.0f);
    float btn_height = std::min(rect.h * 0.4f, 26.0f);
    float base_y = rect.y + rect.h - btn_height - 8.0f;
    layout.left_btn = SDL_FRect{rect.x + 12.0f, base_y, btn_width, btn_height};
    layout.right_btn = SDL_FRect{rect.x + rect.w - btn_width - 12.0f, base_y, btn_width, btn_height};
    float center_left = layout.left_btn.x + layout.left_btn.w + 8.0f;
    float center_right = layout.right_btn.x - 8.0f;
    if (center_right < center_left + 24.0f)
        center_right = center_left + 24.0f;
    layout.value_rect = SDL_FRect{center_left,
                                  base_y,
                                  center_right - center_left,
                                  btn_height};
    return layout;
}

bool point_in_rect(float x, float y, const SDL_FRect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

void compute_quick_value_rects(const SDL_FRect& rect, int count, SDL_FRect* out_rects) {
    if (!out_rects || count <= 0)
        return;
    float area_left = rect.x + rect.w * 0.05f;
    float area_right = rect.x + rect.w - rect.w * 0.05f;
    float area_width = std::max(40.0f, area_right - area_left);
    float spacing = 6.0f;
    float total_spacing = spacing * static_cast<float>(std::max(0, count - 1));
    float btn_width = std::min(92.0f, (area_width - total_spacing) / static_cast<float>(count));
    float btn_height = 22.0f;
    float base_y = rect.y + rect.h * 0.18f;
    for (int i = 0; i < count; ++i) {
        float x = area_left + static_cast<float>(i) * (btn_width + spacing);
        out_rects[i] = SDL_FRect{x, base_y, btn_width, btn_height};
    }
}

bool apply_slider_target(MenuWidget& widget,
                         float target_value,
                         MenuContext& ctx,
                         bool& stack_changed,
                         bool& needs_rebuild) {
    if (!widget.bind_ptr)
        return false;
    float* val = reinterpret_cast<float*>(widget.bind_ptr);
    float range = widget.max - widget.min;
    if (range <= 0.0f)
        return false;
    float step = widget.step > 0.0f ? widget.step : range * 0.01f;
    target_value = std::clamp(target_value, widget.min, widget.max);
    float delta = target_value - *val;
    if (std::fabs(delta) < step * 0.4f)
        return false;
    int steps = static_cast<int>(std::round(delta / step));
    if (steps == 0)
        return false;
    MenuAction action = (steps > 0) ? widget.on_right : widget.on_left;
    if (action.type == MenuActionType::None)
        return false;
    int iterations = std::abs(steps);
    for (int i = 0; i < iterations; ++i) {
        execute_action(action, ctx, stack_changed);
    }
    needs_rebuild = true;
    return true;
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
    if (!widget.text_buffer)
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

bool commit_text_edit() {
    if (!g_text_edit_active || !g_active_text_buffer)
        return false;
    MenuWidget* widget = find_widget(g_text_edit_widget);
    if (!widget || !widget->bind_ptr)
        return false;
    bool modified = false;
    if (widget->type == WidgetType::Slider1D) {
        char* end_ptr = nullptr;
        float parsed = std::strtof(g_active_text_buffer->c_str(), &end_ptr);
        bool parsed_ok = (end_ptr != g_active_text_buffer->c_str());
        if (!parsed_ok) {
            std::string lower = *g_active_text_buffer;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
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
            }
        }
    }
    return modified;
}

bool end_text_edit() {
    if (!g_text_edit_active)
        return false;
    bool modified = commit_text_edit();
    g_text_edit_active = false;
    g_text_edit_widget = kMenuIdInvalid;
    g_active_text_buffer = nullptr;
    if (g_text_input_enabled) {
        SDL_StopTextInput();
        g_text_input_enabled = false;
    }
    if (es)
        lock_mouse_focus_at(es->device_state.mouse_x, es->device_state.mouse_y);
    return modified;
}

bool is_text_edit_widget(WidgetId id) {
    return g_text_edit_active && g_text_edit_widget == id;
}

void set_active_text_buffer(std::string* buffer, int max_len) {
    if (!g_text_edit_active)
        return;
    g_active_text_buffer = buffer;
    g_active_text_max = max_len;
}

WidgetId current_text_widget() {
    return g_text_edit_active ? g_text_edit_widget : kMenuIdInvalid;
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
    if (remembered != kMenuIdInvalid) {
        if (MenuWidget* remembered_widget = find_widget(remembered)) {
            if (is_transient_focus_slot(remembered_widget->slot))
                remembered = kMenuIdInvalid;
        }
    }
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

namespace {

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

} // namespace

void draw_focus_arrows(SDL_Renderer* renderer) {
    if (!g_arrows.initialized || !g_has_focus_rect || !g_has_focus_color)
        return;
    float osc = std::sin(g_arrows.time * 6.0f) * 3.0f;
    SDL_FPoint left_tip{g_arrows.left_pos.x + osc, g_arrows.left_pos.y};
    SDL_FPoint right_tip{g_arrows.right_pos.x - osc, g_arrows.right_pos.y};
    draw_arrow(renderer, left_tip, 1.0f, g_focus_outline_color);
    draw_arrow(renderer, right_tip, -1.0f, g_focus_outline_color);
}

} // namespace menu_system_internal
