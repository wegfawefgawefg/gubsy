#pragma once

#include "engine/ui_layouts.hpp"

#include <SDL2/SDL.h>

struct EngineState;

void layout_editor_draw_grid(SDL_Renderer* renderer, int width, int height, float origin_x,
                             float origin_y, float grid_step);

void layout_editor_draw_layout(const EngineState& engine, SDL_Renderer* renderer,
                               const UILayout& layout, int width, int height, float origin_x,
                               float origin_y, int dragging_index);
