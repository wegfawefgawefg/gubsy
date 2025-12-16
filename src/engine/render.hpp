#pragma once

void render();
void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
void render_alerts(SDL_Renderer* renderer, int width);