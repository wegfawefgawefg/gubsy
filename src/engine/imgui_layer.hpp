#pragma once

#include <SDL2/SDL.h>

bool init_imgui_layer(SDL_Window* window, SDL_Renderer* renderer);
void shutdown_imgui_layer();

void imgui_new_frame();
void imgui_render_layer();
void imgui_process_event(const SDL_Event& event);
bool imgui_is_initialized();
