#include "engine/layout_editor/layout_editor.hpp"

#include "engine/engine_state.hpp"
#include "engine/graphics.hpp"
#include "engine/layout_editor/layout_editor_history.hpp"
#include "engine/layout_editor/layout_editor_hooks.hpp"
#include "engine/layout_editor/layout_editor_interaction.hpp"
#include "engine/layout_editor/layout_editor_internal.hpp"
#include "engine/layout_editor/layout_editor_overlay.hpp"
#include "engine/layout_editor/layout_editor_panel.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <climits>
#include <imgui.h>
#include <string>

namespace layout_editor_internal {

LayoutEditorState& editor_state(EngineState& engine) {
    return engine.layout_editor;
}

const LayoutEditorState& editor_state(const EngineState& engine) {
    return engine.layout_editor;
}

bool has_layouts(const EngineState& engine) {
    return !engine.ui_layouts.layouts.empty();
}

UILayout* selected_layout_mutable(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    if (!has_layouts(engine))
        return nullptr;
    state.selected_layout = std::clamp(state.selected_layout, 0,
                                       static_cast<int>(engine.ui_layouts.layouts.size()) - 1);
    return &engine.ui_layouts.layouts[static_cast<std::size_t>(state.selected_layout)];
}

const UILayout* selected_layout(EngineState& engine) {
    return selected_layout_mutable(engine);
}

void append_status(EngineState& engine, const std::string& text) {
    LayoutEditorState& state = editor_state(engine);
    state.status_text = text;
    state.status_timer = 3.0f;
}

} // namespace layout_editor_internal

using namespace layout_editor_internal;

namespace {

void ensure_history_for_selection(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    UILayout* layout = selected_layout_mutable(engine);
    if (!layout) {
        state.history_initialized = false;
        state.history_layout_id = -1;
        state.history_layout_width = 0;
        state.history_layout_height = 0;
        return;
    }
    if (!state.history_initialized || layout->id != state.history_layout_id ||
        layout->width != state.history_layout_width ||
        layout->height != state.history_layout_height) {
        layout_editor_history_reset(engine, *layout);
        layout_editor_clear_selection(engine);
        state.history_initialized = true;
        state.history_layout_id = layout->id;
        state.history_layout_width = layout->width;
        state.history_layout_height = layout->height;
        state.object_label_index = -1;
        state.object_label_buffer[0] = '\0';
    }
}

bool select_layout_exact(EngineState& engine, int id, int width, int height) {
    LayoutEditorState& state = editor_state(engine);
    for (std::size_t i = 0; i < engine.ui_layouts.layouts.size(); ++i) {
        const auto& layout = engine.ui_layouts.layouts[i];
        if (layout.id == id && layout.width == width && layout.height == height) {
            state.selected_layout = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

void auto_follow_selection(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    if (!state.follow_active_layout)
        return;
    if (state.last_request.valid) {
        if (select_layout_exact(engine, state.last_request.id, state.last_request.width,
                                state.last_request.height)) {
            return;
        }
    }
    const Graphics* graphics = current_graphics(engine);
    if (!graphics || !has_layouts(engine))
        return;
    int target_w = static_cast<int>(graphics->render_dims.x);
    int target_h = static_cast<int>(graphics->render_dims.y);
    if (target_w <= 0 || target_h <= 0)
        return;
    int best_idx = state.selected_layout;
    int best_score = INT_MAX;
    for (std::size_t i = 0; i < engine.ui_layouts.layouts.size(); ++i) {
        const auto& layout = engine.ui_layouts.layouts[i];
        int dw = layout.width - target_w;
        int dh = layout.height - target_h;
        int score = dw * dw + dh * dh;
        if (score < best_score) {
            best_score = score;
            best_idx = static_cast<int>(i);
        }
    }
    state.selected_layout = best_idx;
}

glayout::Viewport to_glayout_viewport(const LayoutEditorViewport& viewport) {
    return glayout::Viewport{viewport.origin_x, viewport.origin_y, viewport.width, viewport.height};
}

int generate_editor_object_id(const glayout::Layout&, void*) {
    return generate_ui_object_id();
}

void mark_editor_changed(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    state.layout_dirty = true;
    state.object_label_index = -1;
}

void handle_hotkeys(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    if (!ImGui::GetCurrentContext())
        return;
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_L) && io.KeyCtrl) {
        state.active = !state.active;
        if (!state.active)
            append_status(engine, "Layout editor deactivated");
        else
            append_status(engine, "Layout editor activated");
    }
    if (!state.active)
        return;
    if (has_layouts(engine) && ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
        if (const UILayout* layout = selected_layout(engine)) {
            if (save_ui_layout(*layout)) {
                glayout::editor_mark_saved(state.editor);
                append_status(engine, "Layout saved");
            } else {
                append_status(engine, "Failed to save layout");
            }
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G))
        state.editor.snap_enabled = !state.editor.snap_enabled;
    if (ImGui::IsKeyPressed(ImGuiKey_Equal))
        state.editor.grid_step = std::max(0.01f, state.editor.grid_step - 0.01f);
    if (ImGui::IsKeyPressed(ImGuiKey_Minus))
        state.editor.grid_step = std::min(0.5f, state.editor.grid_step + 0.01f);

    UILayout* layout = selected_layout_mutable(engine);
    if (!layout)
        return;

    float nudge = io.KeyShift ? 0.05f : 0.01f;
    bool mouse_down = (engine.device_state.mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    bool mouse_captured = io.WantCaptureMouse;

    glayout::EditorInput input;
    input.mouse_x = static_cast<float>(engine.device_state.mouse_x);
    input.mouse_y = static_cast<float>(engine.device_state.mouse_y);
    input.left_down = mouse_down && !mouse_captured;
    input.ctrl = io.KeyCtrl;
    input.shift = io.KeyShift;
    input.key_undo = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false);
    input.key_redo = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false);
    input.key_copy = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false);
    input.key_paste = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false);
    input.key_delete = ImGui::IsKeyPressed(ImGuiKey_Delete, false);
    input.generate_object_id = generate_editor_object_id;

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
        input.nudge_x -= nudge;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
        input.nudge_x += nudge;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
        input.nudge_y -= nudge;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
        input.nudge_y += nudge;

    glayout::EditorFrameResult result = glayout::editor_begin_frame(
        state.editor, *layout, input, to_glayout_viewport(layout_editor_get_viewport(engine)));
    if (result.changed)
        mark_editor_changed(engine);

    if (input.key_undo && result.changed)
        append_status(engine, "Undo");
    else if (input.key_redo && result.changed)
        append_status(engine, "Redo");
}

} // namespace

void layout_editor_notify_active_layout(EngineState& engine, int layout_id, int width, int height) {
    LayoutEditorState& state = editor_state(engine);
    state.last_request.valid = true;
    state.last_request.id = layout_id;
    state.last_request.width = width;
    state.last_request.height = height;
}

bool layout_editor_consume_dirty_flag(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    bool dirty = state.layout_dirty;
    state.layout_dirty = false;
    return dirty;
}

void layout_editor_begin_frame(EngineState& engine, float dt) {
    LayoutEditorState& state = editor_state(engine);
    if (state.follow_active_layout)
        auto_follow_selection(engine);
    ensure_history_for_selection(engine);
    handle_hotkeys(engine);
    if (state.active)
        layout_editor_render_panel(engine, dt);
    else
        state.status_timer = std::max(0.0f, state.status_timer - dt);
}

bool layout_editor_is_active(const EngineState& engine) {
    return editor_state(engine).active;
}

bool layout_editor_wants_input(const EngineState& engine) {
    return editor_state(engine).active;
}

void layout_editor_render(EngineState& engine, SDL_Renderer* renderer, int screen_width,
                          int screen_height, float origin_x, float origin_y) {
    const LayoutEditorState& state = editor_state(engine);
    if (!state.active || !renderer)
        return;
    if (!has_layouts(engine))
        return;
    layout_editor_set_viewport(engine, LayoutEditorViewport{origin_x, origin_y,
                                                            static_cast<float>(screen_width),
                                                            static_cast<float>(screen_height)});
    layout_editor_draw_grid(renderer, screen_width, screen_height, origin_x, origin_y,
                            state.editor.grid_step);
    if (const UILayout* layout = selected_layout(engine)) {
        int dragging_idx = layout_editor_dragging_index(engine);
        layout_editor_draw_layout(engine, renderer, *layout, screen_width, screen_height, origin_x,
                                  origin_y, dragging_idx);
    }
}

void layout_editor_shutdown(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    state.active = false;
    state.status_text.clear();
    state.status_timer = 0.0f;
    layout_editor_history_shutdown(engine);
    state.history_initialized = false;
}
