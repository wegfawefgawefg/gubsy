#pragma once

#include "glm/glm.hpp"

// buttons 
bool was_pressed(int player_index, int game_button);
bool was_released(int player_index, int game_button);
bool is_down(int player_index, int game_button);
// axes
float get_1d_analog(int player_index, int game_axis);
float get_1d_analog_delta(int player_index, int game_axis);
glm::vec2 get_2d_analog(int player_index, int game_axis);
glm::vec2 get_2d_analog_delta(int player_index, int game_axis);

// mouse 
glm::vec2 get_mouse_pos(int player_index);
glm::vec2 get_mouse_delta(int player_index);
glm::vec2 get_mouse_pos_window(int player_index);
glm::vec2 get_mouse_pos_screen(int player_index);
int get_mouse_wheel_delta(int player_index);

