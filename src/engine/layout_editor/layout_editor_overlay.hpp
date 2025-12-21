#pragma once

#include <SDL2/SDL.h>

struct UILayout;

void layout_editor_draw_grid(SDL_Renderer* renderer,
                             int width,
                             int height,
                             float origin_x,
                             float origin_y,
                             float grid_step);

void layout_editor_draw_layout(SDL_Renderer* renderer,
                               const UILayout& layout,
                               int width,
                               int height,
                               float origin_x,
                               float origin_y,
                               int dragging_index);
