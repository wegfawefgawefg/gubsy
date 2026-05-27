#include "input_sources.hpp"
#include "src/engine_state.hpp"
#include "src/imgui_layer.hpp"
#include "src/input_system.hpp"
#include "src/menu/menu_system.hpp"

#include <SDL3/SDL_events.h>

void process_gubsy_sdl_event(EngineState& engine, const SDL_Event& ev) {
    imgui_process_event(ev);
    switch (ev.type) {
    case SDL_EVENT_QUIT:
        engine.running = false;
        break;
    case SDL_EVENT_KEY_DOWN:
        if (ev.key.key == SDLK_BACKSPACE && !imgui_want_capture_keyboard()) {
            bool ctrl_held = (ev.key.mod & SDL_KMOD_CTRL) != 0;
            menu_system_handle_backspace(engine, ctrl_held);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (!imgui_want_capture_mouse())
            accumulate_mouse_wheel_delta(engine, static_cast<int>(ev.wheel.y));
        break;
    case SDL_EVENT_TEXT_INPUT:
        if (!imgui_want_capture_keyboard() && !imgui_want_text_input())
            menu_system_handle_text_input(engine, ev.text.text);
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        on_device_added(engine, static_cast<int>(ev.gdevice.which));
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        on_device_removed(engine, static_cast<int>(ev.gdevice.which));
        break;
    default:
        break;
    }
}

void update_gubsy_device_inputs_system_from_sdl_events(EngineState& engine) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        process_gubsy_sdl_event(engine, ev);
    }
}
