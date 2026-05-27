#pragma once

struct EngineState;
union SDL_Event;

/** Processes SDL events for this frame and updates Gubsy input/menu state. */
void update_gubsy_device_inputs_system_from_sdl_events(EngineState& engine);
void process_gubsy_sdl_event(EngineState& engine, const SDL_Event& ev);
