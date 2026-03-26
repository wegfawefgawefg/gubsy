#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/menu/menu_runtime_state.hpp"
#include "engine/menu/menu_system.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_types.hpp"
#include "engine/menu_layout_ids.hpp"
#include "engine/ui_layouts.hpp"

struct EngineState;

inline constexpr float kSliderOptionButtonWidth = 32.0f;
inline constexpr float kSliderOptionButtonSpacing = 8.0f;

namespace menu_system_internal {

MenuRuntimeState& runtime_state(EngineState& engine);
const MenuRuntimeState& runtime_state(const EngineState& engine);

constexpr float kRepeatDelay = 0.32f;
constexpr float kRepeatInterval = 0.08f;

void play_focus_sound(EngineState& engine);
void play_confirm_sound(EngineState& engine);
void play_cant_sound(EngineState& engine);
void play_left_sound(EngineState& engine);
void play_right_sound(EngineState& engine);

void lock_mouse_focus_at(MenuRuntimeState& state, int x, int y);
void ensure_mouse_lock(MenuRuntimeState& state, int x, int y);
void unlock_mouse_focus_if_moved(MenuRuntimeState& state, int x, int y);
void unlock_mouse_focus_now(MenuRuntimeState& state);

MenuWidget* find_widget(MenuRuntimeState& state, WidgetId id);
const MenuWidget* find_widget(const MenuRuntimeState& state, WidgetId id);
MenuWidget* find_widget_by_slot(MenuRuntimeState& state, UILayoutObjectId slot);
SDL_FRect* find_widget_rect(MenuRuntimeState& state, WidgetId id);
bool is_transient_focus_slot(UILayoutObjectId slot);

struct SliderLayout {
    float track_left{0.0f};
    float track_right{0.0f};
    float track_y{0.0f};
    bool has_input{false};
    SDL_FRect input_rect{};
    bool has_buttons{false};
    SDL_FRect left_btn{};
    SDL_FRect right_btn{};
};
struct OptionLayout {
    SDL_FRect left_btn{};
    SDL_FRect right_btn{};
    SDL_FRect value_rect{};
    bool has_primary_input{false};
    SDL_FRect primary_input{};
    bool has_secondary_input{false};
    SDL_FRect secondary_input{};
};

SliderLayout compute_slider_layout(const MenuWidget& widget, const SDL_FRect& rect);
OptionLayout compute_option_layout(const MenuWidget& widget, const SDL_FRect& rect);
bool point_in_rect(float x, float y, const SDL_FRect& rect);

WidgetId resolve_focus(const MenuRuntimeState& state, WidgetId target);
WidgetId first_selectable_widget(const MenuRuntimeState& state);

void reset_repeat_state(NavRepeatState& state);
void update_repeat(bool down, NavRepeatState& state, bool& trigger, float dt);

SDL_FRect rect_from_object(const UIObject& obj, int width, int height);

int measure_text_width(const EngineState& engine, const char* text);
void draw_text_with_clip(const EngineState& engine,
                         SDL_Renderer* renderer,
                         const char* text,
                         int x,
                         int y,
                         SDL_Color color,
                         const SDL_Rect* clip);
void begin_text_edit(MenuRuntimeState& state, MenuWidget& widget, bool use_aux_buffer = false);
bool commit_text_edit(MenuRuntimeState& state);
bool end_text_edit(MenuRuntimeState& state);
bool is_text_edit_widget(const MenuRuntimeState& state, WidgetId id);
void set_active_text_buffer(MenuRuntimeState& state, std::string* buffer, int max_len);
WidgetId current_text_widget(const MenuRuntimeState& state);

bool execute_action(MenuRuntimeState& state,
                    const MenuAction& action,
                    MenuContext& ctx,
                    bool& stack_changed);
void rebuild_cache(MenuRuntimeState& state,
                   MenuManager::ScreenInstance& inst,
                   MenuContext& ctx);

void update_arrows(MenuRuntimeState& state, float dt);
void draw_focus_arrows(const MenuRuntimeState& state, SDL_Renderer* renderer);

} // namespace menu_system_internal
