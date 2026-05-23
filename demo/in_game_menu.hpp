#pragma once

#include <SDL2/SDL.h>

struct EngineState;

void in_game_menu_reset(EngineState& engine);
bool in_game_menu_open();
bool in_game_menu_blocks_game_input();
void in_game_menu_process_inputs(EngineState& engine);
void in_game_menu_render(EngineState& engine, SDL_Renderer* renderer, int width, int height);
