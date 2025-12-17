#pragma once

#include <SDL2/SDL.h>

void render();
void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
void render_alerts(SDL_Renderer* renderer, int width);
void fill_and_outline(SDL_Renderer* renderer, const SDL_FRect& rect,
                      SDL_Color fill, SDL_Color border);