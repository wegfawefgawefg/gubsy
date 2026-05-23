#include <SDL2/SDL_events.h>
#include "src/engine_state.hpp"
#include "input_sources.hpp"
#include "src/input_system.hpp"
#include "src/imgui_layer.hpp"
#include "src/menu/menu_system.hpp"

void update_gubsy_device_inputs_system_from_sdl_events(EngineState& engine) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        imgui_process_event(ev);
        switch (ev.type) {
            case SDL_QUIT:
                engine.running = false;
                break;
            case SDL_KEYDOWN:
                if (ev.key.key == SDLK_BACKSPACE &&
                    !imgui_want_capture_keyboard()) {
                    bool ctrl_held = (ev.key.mod & KMOD_CTRL) != 0;
                    menu_system_handle_backspace(engine, ctrl_held);
                }
                break;
            case SDL_KEYUP:
                break;
            case SDL_MOUSEMOTION:
                break;
            case SDL_MOUSEWHEEL:
                if (!imgui_want_capture_mouse())
                    accumulate_mouse_wheel_delta(engine, static_cast<int>(ev.wheel.y));
                break;
            case SDL_TEXTINPUT:
                if (!imgui_want_capture_keyboard() && !imgui_want_text_input())
                    menu_system_handle_text_input(engine, ev.text.text);
                break;
            case SDL_CONTROLLERDEVICEADDED:
                on_device_added(engine, static_cast<int>(ev.gdevice.which));
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                on_device_removed(engine, static_cast<int>(ev.gdevice.which));
                break;
            default:
                break;
        }
    }
}
