#pragma once

#include <cstdbool>

// Each helper draws a named ImGui window when the corresponding toggle is true.
// The pointer follows ImGui's Begin API expectations (may be null).
void imgui_debug_render_players_window(bool* open_flag);
void imgui_debug_render_binds_window(bool* open_flag);
void imgui_debug_render_layout_window(bool* open_flag);
void imgui_debug_render_video_window(bool* open_flag);
