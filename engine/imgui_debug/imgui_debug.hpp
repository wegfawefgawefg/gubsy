#pragma once

struct EngineState;

struct ImguiDebugWindowDef {
    const char* label{nullptr};
    int hotkey{0};
    const char* hotkey_label{nullptr};
    void (*render)(bool* open_flag){nullptr};
};

// Per-frame hook after imgui_new_frame().
void imgui_debug_begin_frame(float dt);

// Called before imgui_render_layer() to draw debug UI.
void imgui_debug_render(EngineState& engine);

void imgui_debug_register_window(const ImguiDebugWindowDef& def);

// Cleanup if necessary (currently a stub).
void imgui_debug_shutdown();
