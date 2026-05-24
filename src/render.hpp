#pragma once

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <string>

struct EngineState;

struct ScreenSpace {
    float scale{64.0f};
    float cx{0.0f};
    float cy{0.0f};
};

void render(EngineState& engine);
bool render_frame_to_window(EngineState& engine);
void present_frame(EngineState& engine);
void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
void render_alerts(const EngineState& engine, SDL_Renderer* renderer, int width);
void fill_and_outline(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color fill,
                      SDL_Color border);

SDL_Color color_from_vec3(const glm::vec3& color, Uint8 alpha = 255);
SDL_FRect rect_for(const glm::vec2& pos, const glm::vec2& half_size, const ScreenSpace& space);
ScreenSpace make_space(int width, int height);
glm::vec3 brighten(const glm::vec3& base, float amount);
