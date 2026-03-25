#pragma once

#include "engine/menu/menu_manager.hpp"

#include <SDL2/SDL_render.h>

struct EngineState;
struct State;

void render_menu(MenuManager& manager,
                 EngineState& engine,
                 State& game,
                 SDL_Renderer* renderer,
                 int width,
                 int height);
