#pragma once

#include "engine/device_state.hpp"
#include "game/input_frame.hpp"

using BuildInputFrameFn = void (*)(int player_index, const DeviceState&, InputFrame&);

void register_input_frame_builder(BuildInputFrameFn fn);
void update_device_state_from_sdl();
void accumulate_mouse_wheel_delta(int delta);
void build_input_frames_for_step();
const InputFrame& current_input_frame(int player_index);
const InputFrame& previous_input_frame(int player_index);
