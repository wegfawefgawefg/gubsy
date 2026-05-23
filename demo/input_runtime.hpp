#pragma once

#include "src/engine_state.hpp"
#include "demo/input_frame.hpp"

void build_input_frames_for_step(EngineState& engine, void* app_context);
const InputFrame& current_input_frame(int player_index);
const InputFrame& previous_input_frame(int player_index);
