#include <SDL2/SDL_events.h>
#include <engine/globals.hpp>
#include "input_sources.hpp"
#include "engine/input_system.hpp"
#include "engine/imgui_layer.hpp"
#include "engine/menu/menu_system.hpp"

void update_gubsy_device_inputs_system_from_sdl_events(){
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        imgui_process_event(ev);
        switch (ev.type) {
            case SDL_QUIT:
                es->running = false;
                break;
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_BACKSPACE &&
                    !imgui_want_capture_keyboard())
                    menu_system_handle_backspace();
                break;
            case SDL_KEYUP:
                break;
            case SDL_MOUSEMOTION:
                break;
            case SDL_MOUSEWHEEL:
                if (!imgui_want_capture_mouse())
                    accumulate_mouse_wheel_delta(ev.wheel.y);
                break;
            case SDL_TEXTINPUT:
                if (!imgui_want_capture_keyboard() && !imgui_want_text_input())
                    menu_system_handle_text_input(ev.text.text);
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
