#pragma once

#include <SDL2/SDL.h>

void layout_editor_begin_frame(float dt);
bool layout_editor_is_active();
bool layout_editor_wants_input();
void layout_editor_render(SDL_Renderer* renderer,
                          int screen_width,
                          int screen_height,
                          float origin_x = 0.0f,
                          float origin_y = 0.0f);
void layout_editor_shutdown();
