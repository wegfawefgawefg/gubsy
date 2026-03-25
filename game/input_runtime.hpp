#pragma once

#include "game/input_frame.hpp"
void build_input_frames_for_step();
const InputFrame& current_input_frame(int player_index);
const InputFrame& previous_input_frame(int player_index);
