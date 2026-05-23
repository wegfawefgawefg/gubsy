#pragma once

#include <SDL2/SDL.h>

bool init_imgui_layer(SDL_Window* window, SDL_Renderer* renderer);
void shutdown_imgui_layer();

void imgui_new_frame();
void imgui_render_layer();
void imgui_process_event(const SDL_Event& event);
bool imgui_is_initialized();
void imgui_set_scale(float scale);
void imgui_adjust_scale(float delta);
float imgui_get_scale();
bool imgui_want_capture_mouse();
bool imgui_want_capture_keyboard();
bool imgui_want_text_input();
