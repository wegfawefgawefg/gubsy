#pragma once

#include "gubsy/menu/manager.hpp"

#include <SDL3/SDL.h>

struct MenuInputState {
    bool up{false};
    bool down{false};
    bool left{false};
    bool right{false};
    bool select{false};
    bool back{false};
    bool page_prev{false};
    bool page_next{false};
};

struct EngineState;

void menu_system_set_input(EngineState& engine, const MenuInputState& input);
void menu_system_update(EngineState& engine, float dt, int screen_width, int screen_height);
void menu_system_render(EngineState& engine, SDL_Renderer* renderer, int screen_width, int screen_height);
void menu_system_reset(EngineState& engine);
bool menu_system_active(const EngineState& engine);
void menu_system_handle_text_input(EngineState& engine, const char* text);
void menu_system_handle_backspace(EngineState& engine, bool clear_all = false);
