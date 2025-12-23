#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/menu/menu_system.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_types.hpp"
#include "engine/ui_layouts.hpp"
#include "game/ui_layout_ids.hpp"

namespace menu_system_internal {

struct RuntimeCache {
    UILayoutId layout{kMenuIdInvalid};
    int width{0};
    int height{0};
    std::vector<MenuWidget> widgets;
    std::vector<SDL_FRect> rects;
};

extern RuntimeCache g_cache;
extern WidgetId g_focus;
extern MenuInputState g_prev_input;
extern MenuInputState g_current_input;
extern bool g_active;
extern bool g_prev_mouse_down;
extern std::string* g_active_text_buffer;
extern int g_active_text_max;
extern bool g_text_input_enabled;
extern SDL_FRect g_focus_rect;
extern bool g_has_focus_rect;
extern SDL_Color g_focus_outline_color;
extern bool g_has_focus_color;
extern bool g_text_edit_active;
extern WidgetId g_text_edit_widget;
extern float g_caret_time;
extern std::unordered_map<MenuScreenId, WidgetId> g_last_focus;
extern MenuScreenId g_current_screen;
extern bool g_allow_mouse_focus;
extern bool g_mouse_focus_locked;
extern int g_mouse_focus_lock_x;
extern int g_mouse_focus_lock_y;

struct NavRepeatState {
    bool active{false};
    bool repeated{false};
    float timer{0.0f};
};

extern NavRepeatState g_repeat_up;
extern NavRepeatState g_repeat_down;
extern NavRepeatState g_repeat_left;
extern NavRepeatState g_repeat_right;

constexpr float kRepeatDelay = 0.32f;
constexpr float kRepeatInterval = 0.08f;

extern WidgetId g_slider_drag_id;

struct FocusArrowState {
    SDL_FPoint left_pos{0.0f, 0.0f};
    SDL_FPoint right_pos{0.0f, 0.0f};
    SDL_FPoint left_target{0.0f, 0.0f};
    SDL_FPoint right_target{0.0f, 0.0f};
    bool initialized{false};
    float time{0.0f};
};

extern FocusArrowState g_arrows;

void play_focus_sound();
void play_confirm_sound();
void play_cant_sound();
void play_left_sound();
void play_right_sound();

void lock_mouse_focus_at(int x, int y);
void ensure_mouse_lock(int x, int y);
void unlock_mouse_focus_if_moved(int x, int y);
void unlock_mouse_focus_now();

MenuWidget* find_widget(WidgetId id);
MenuWidget* find_widget_by_slot(UILayoutObjectId slot);
SDL_FRect* find_widget_rect(WidgetId id);
bool is_transient_focus_slot(UILayoutObjectId slot);

struct SliderLayout {
    float track_left{0.0f};
    float track_right{0.0f};
    float track_y{0.0f};
    bool has_input{false};
    SDL_FRect input_rect{};
};

SliderLayout compute_slider_layout(const MenuWidget& widget, const SDL_FRect& rect);
bool apply_slider_target(MenuWidget& widget,
                         float target_value,
                         MenuContext& ctx,
                         bool& stack_changed,
                         bool& needs_rebuild);

WidgetId resolve_focus(WidgetId target);
WidgetId first_selectable_widget();

void reset_repeat_state(NavRepeatState& state);
void update_repeat(bool down, NavRepeatState& state, bool& trigger, float dt);

SDL_FRect rect_from_object(const UIObject& obj, int width, int height);

int measure_text_width(const char* text);
void draw_text_with_clip(SDL_Renderer* renderer,
                         const char* text,
                         int x,
                         int y,
                         SDL_Color color,
                         const SDL_Rect* clip);

void begin_text_edit(MenuWidget& widget);
bool commit_text_edit();
bool end_text_edit();
bool is_text_edit_widget(WidgetId id);
void set_active_text_buffer(std::string* buffer, int max_len);
WidgetId current_text_widget();

bool execute_action(const MenuAction& action, MenuContext& ctx, bool& stack_changed);
void rebuild_cache(MenuManager::ScreenInstance& inst, MenuContext& ctx);

void update_arrows(float dt);
void draw_focus_arrows(SDL_Renderer* renderer);

} // namespace menu_system_internal
