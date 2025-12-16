#include "engine/ui/menu/menu.hpp"
#include "engine/globals.hpp"

#include <SDL2/SDL_render.h>

void title_step() {
}

void title_process_inputs() {
}

void title_draw() {
    SDL_Renderer* renderer = gg->renderer;
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_SetRenderDrawColor(renderer, 12, 10, 18, 255);
    SDL_RenderClear(renderer);
    render_menu(width, height);
    SDL_RenderPresent(renderer);
}
