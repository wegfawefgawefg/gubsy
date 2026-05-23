#pragma once

#include <cstdbool>

struct EngineState;

// Each helper draws a named ImGui window when the corresponding toggle is true.
// The pointer follows ImGui's Begin API expectations (may be null).
void imgui_debug_render_binds_window(EngineState& engine, bool* open_flag);
void imgui_debug_render_layout_window(EngineState& engine, bool* open_flag);
void imgui_debug_render_video_window(EngineState& engine, bool* open_flag);
