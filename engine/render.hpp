#pragma once

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <string>

struct ScreenSpace {
    float scale{64.0f};
    float cx{0.0f};
    float cy{0.0f};
};

void render();
void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
void render_alerts(SDL_Renderer* renderer, int width);
void fill_and_outline(SDL_Renderer* renderer, const SDL_FRect& rect,
                      SDL_Color fill, SDL_Color border);

SDL_Color color_from_vec3(const glm::vec3& color, Uint8 alpha = 255);
SDL_FRect rect_for(const glm::vec2& pos, const glm::vec2& half_size, const ScreenSpace& space);
ScreenSpace make_space(int width, int height);
glm::vec3 brighten(const glm::vec3& base, float amount);
