#pragma once

#include "engine/device_state.hpp"

struct EngineState;

void update_device_state_from_sdl(EngineState& engine);
void accumulate_mouse_wheel_delta(EngineState& engine, int delta);
