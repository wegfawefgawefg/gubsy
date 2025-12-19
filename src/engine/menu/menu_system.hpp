#pragma once

#include "engine/menu/menu_manager.hpp"

#include <SDL2/SDL.h>

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

void menu_system_set_input(const MenuInputState& input);
void menu_system_update(float dt, int screen_width, int screen_height);
void menu_system_render(SDL_Renderer* renderer);
void menu_system_reset();
bool menu_system_active();
void menu_system_handle_text_input(const char* text);
void menu_system_handle_backspace();
