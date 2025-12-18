#include <SDL2/SDL_events.h>
#include <engine/globals.hpp>
#include "input_sources.hpp"
#include "engine/input_system.hpp"

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
                accumulate_mouse_wheel_delta(ev.wheel.y);
                break;
            case SDL_CONTROLLERDEVICEADDED:
                on_device_added(ev.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                on_device_removed(ev.cdevice.which);
                break;
            default:
                break;
        }
    }
}
