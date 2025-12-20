#pragma once

// Per-frame hook after imgui_new_frame().
void imgui_debug_begin_frame(float dt);

// Called before imgui_render_layer() to draw debug UI.
void imgui_debug_render();

// Cleanup if necessary (currently a stub).
void imgui_debug_shutdown();
