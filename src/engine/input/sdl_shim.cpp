#include <SDL2/SDL_events.h>
#include <globals.hpp>

void update_gubsy_device_inputs_system_from_sdl_events(){
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                es->running = false;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                // Handled in process_event
                break;
            case SDL_MOUSEMOTION:
                break;
            case SDL_MOUSEWHEEL:
                break;
            default:
                break;
        }
    }
}