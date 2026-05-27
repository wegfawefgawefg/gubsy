#pragma once

#include <SDL3/SDL.h>

struct EngineState;

void layout_editor_begin_frame(EngineState& engine, float dt);
bool layout_editor_is_active(const EngineState& engine);
bool layout_editor_wants_input(const EngineState& engine);
void layout_editor_render(EngineState& engine,
                          SDL_Renderer* renderer,
                          int screen_width,
                          int screen_height,
                          float origin_x = 0.0f,
                          float origin_y = 0.0f);
void layout_editor_shutdown(EngineState& engine);
