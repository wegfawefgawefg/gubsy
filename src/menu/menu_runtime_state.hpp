#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "src/menu/menu_system.hpp"
#include "src/menu/menu_types.hpp"
#include "src/menu_layout_ids.hpp"

namespace menu_system_internal {

struct RuntimeCache {
    UILayoutId layout{kMenuIdInvalid};
    int width{0};
    int height{0};
    std::vector<MenuWidget> widgets;
    std::vector<SDL_FRect> rects;
};

struct NavRepeatState {
    bool active{false};
    bool repeated{false};
    float timer{0.0f};
};

struct FocusArrowState {
    SDL_FPoint left_pos{0.0f, 0.0f};
    SDL_FPoint right_pos{0.0f, 0.0f};
    SDL_FPoint left_target{0.0f, 0.0f};
    SDL_FPoint right_target{0.0f, 0.0f};
    bool initialized{false};
    float time{0.0f};
};

struct MenuRuntimeState {
    RuntimeCache cache{};
    WidgetId focus{kMenuIdInvalid};
    MenuInputState prev_input{};
    MenuInputState current_input{};
    bool active{false};
    bool prev_mouse_down{false};
    std::string* active_text_buffer{nullptr};
    int active_text_max{0};
    bool text_input_enabled{false};
    SDL_FRect focus_rect{};
    bool has_focus_rect{false};
    SDL_Color focus_outline_color{120, 170, 255, 255};
    bool has_focus_color{false};
    bool text_edit_active{false};
    WidgetId text_edit_widget{kMenuIdInvalid};
    float caret_time{0.0f};
    bool text_edit_using_aux{false};
    std::unordered_map<MenuScreenId, WidgetId> last_focus{};
    MenuScreenId current_screen{kMenuIdInvalid};
    bool allow_mouse_focus{true};
    bool mouse_focus_locked{false};
    int mouse_focus_lock_x{0};
    int mouse_focus_lock_y{0};
    int last_mouse_x{0};
    int last_mouse_y{0};
    NavRepeatState repeat_up{};
    NavRepeatState repeat_down{};
    NavRepeatState repeat_left{};
    NavRepeatState repeat_right{};
    WidgetId slider_drag_id{kMenuIdInvalid};
    bool slider_drag_value_valid{false};
    float slider_drag_value{0.0f};
    bool slider_commit_pending{false};
    FocusArrowState arrows{};
};

} // namespace menu_system_internal
