#pragma once

#include "src/menu/menu_manager.hpp"

#include <SDL3/SDL_render.h>

struct EngineState;

void render_menu(MenuManager& manager,
                 EngineState& engine,
                 SDL_Renderer* renderer,
                 int width,
                 int height);
