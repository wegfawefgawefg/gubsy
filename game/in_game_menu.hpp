#pragma once

#include <SDL2/SDL.h>

void in_game_menu_reset();
bool in_game_menu_open();
bool in_game_menu_blocks_game_input();
void in_game_menu_process_inputs();
void in_game_menu_render(SDL_Renderer* renderer, int width, int height);
