#include <algorithm>
#include <cctype>
#include "mode_registry.hpp"
#include <engine/mode_registry.hpp>
#include <SDL_keyboard.h>
#include <engine/globals.hpp>


void process_inputs() {
    // Dispatch to current mode's input processor
    if (const ModeDesc* mode = find_mode(es->mode)) {
        if (mode->process_inputs_fn) {
            mode->process_inputs_fn();
        }
    }

    // toggle debug input device overlay: ctrl-alt-i
    const Uint8* keystate = SDL_GetKeyboardState(nullptr);
    if (keystate[SDL_SCANCODE_LCTRL] && keystate[SDL_SCANCODE_LALT] && keystate[SDL_SCANCODE_I]) {
        es->draw_input_device_overlay = !es->draw_input_device_overlay;
    }
}

