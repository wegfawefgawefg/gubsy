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
#include <array>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <limits>
#include <string>
#include <vector>

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

constexpr float kClipboardNudge = 0.02f;

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

bool copy_selection_to_clipboard(EngineState& engine, const UILayout& layout) {
    LayoutEditorState& state = editor_state(engine);
    const auto& sel = layout_editor_selection_indices(engine);
    if (sel.empty())
        return false;
    state.clipboard.objects.clear();
    for (int index : sel) {
        if (index < 0 || index >= static_cast<int>(layout.objects.size()))
            continue;
        state.clipboard.objects.push_back(layout.objects[static_cast<std::size_t>(index)]);
    }
    return !state.clipboard.objects.empty();
}

bool paste_clipboard(EngineState& engine, UILayout& layout) {
    LayoutEditorState& state = editor_state(engine);
    if (state.clipboard.objects.empty())
        return false;
    std::vector<int> new_indices;
    new_indices.reserve(state.clipboard.objects.size());
    for (const auto& obj : state.clipboard.objects) {
        UIObject copy = obj;
        copy.id = generate_ui_object_id();
        copy.rect.x = std::clamp(copy.rect.x + kClipboardNudge, 0.0f, 1.0f - copy.rect.w);
        copy.rect.y = std::clamp(copy.rect.y + kClipboardNudge, 0.0f, 1.0f - copy.rect.h);
        layout.objects.push_back(copy);
        new_indices.push_back(static_cast<int>(layout.objects.size()) - 1);
    }
    layout_editor_clear_selection(engine);
    for (int idx : new_indices)
        layout_editor_add_to_selection(engine, idx);
    return true;
}

bool delete_selection(EngineState& engine, UILayout& layout) {
    const auto& sel = layout_editor_selection_indices(engine);
    if (sel.empty())
        return false;
    std::vector<int> to_remove = sel;
    std::sort(to_remove.begin(), to_remove.end());
    for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
        if (*it < 0 || *it >= static_cast<int>(layout.objects.size()))
            continue;
        layout.objects.erase(layout.objects.begin() + *it);
    }
    layout_editor_clear_selection(engine);
    return true;
}

bool translate_selection(const EngineState& engine, UILayout& layout, float dx, float dy) {
    const auto& sel = layout_editor_selection_indices(engine);
    if (sel.empty())
        return false;
    float max_dx_positive = std::numeric_limits<float>::max();
    float max_dx_negative = std::numeric_limits<float>::max();
    float max_dy_positive = std::numeric_limits<float>::max();
    float max_dy_negative = std::numeric_limits<float>::max();
    for (int index : sel) {
        if (index < 0 || index >= static_cast<int>(layout.objects.size()))
            continue;
        const auto& obj = layout.objects[static_cast<std::size_t>(index)];
        max_dx_positive = std::min(max_dx_positive, 1.0f - (obj.rect.x + obj.rect.w));
        max_dx_negative = std::min(max_dx_negative, obj.rect.x);
        max_dy_positive = std::min(max_dy_positive, 1.0f - (obj.rect.y + obj.rect.h));
        max_dy_negative = std::min(max_dy_negative, obj.rect.y);
    }
    if (dx > 0.0f)
        dx = std::min(dx, max_dx_positive);
    else if (dx < 0.0f)
        dx = std::max(dx, -max_dx_negative);
    if (dy > 0.0f)
        dy = std::min(dy, max_dy_positive);
    else if (dy < 0.0f)
        dy = std::max(dy, -max_dy_negative);
    if (std::fabs(dx) < 1e-6f && std::fabs(dy) < 1e-6f)
        return false;
    for (int index : sel) {
        if (index < 0 || index >= static_cast<int>(layout.objects.size()))
            continue;
        auto& obj = layout.objects[static_cast<std::size_t>(index)];
        obj.rect.x = std::clamp(obj.rect.x + dx, 0.0f, 1.0f - obj.rect.w);
        obj.rect.y = std::clamp(obj.rect.y + dy, 0.0f, 1.0f - obj.rect.h);
    }
    return true;
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

void handle_mouse_input(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    if (!state.active)
        return;
    if (ImGui::GetCurrentContext()) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            state.mouse_was_down = io.MouseDown[0];
            return;
        }
    }
    LayoutEditorViewport viewport = layout_editor_get_viewport(engine);
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return;
    UILayout* layout = selected_layout_mutable(engine);
    if (!layout)
        return;

    float mouse_x = static_cast<float>(engine.device_state.mouse_x);
    float mouse_y = static_cast<float>(engine.device_state.mouse_y);
    bool mouse_down = (engine.device_state.mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    bool shift = false;
    bool ctrl = false;
    if (ImGui::GetCurrentContext()) {
        const ImGuiIO& io = ImGui::GetIO();
        shift = io.KeyShift;
        ctrl = io.KeyCtrl;
    }

    if (mouse_down && !state.mouse_was_down) {
        state.drag_dirty = false;
        HitResult hit{};
        if (layout_editor_hit_test(engine, *layout, viewport, mouse_x, mouse_y, hit)) {
            if (hit.target == HitTarget::Object && hit.object_index >= 0) {
                bool was_selected = layout_editor_is_selected(engine, hit.object_index);
                int selection_count = layout_editor_selection_count(engine);
                if (ctrl) {
                    if (was_selected)
                        layout_editor_remove_from_selection(engine, hit.object_index);
                    else
                        layout_editor_add_to_selection(engine, hit.object_index);
                } else if (shift) {
                    layout_editor_add_to_selection(engine, hit.object_index);
                } else if (!was_selected || selection_count <= 1) {
                    layout_editor_select_single(engine, hit.object_index);
                }
                selection_count = layout_editor_selection_count(engine);

                if (layout_editor_selection_count(engine) > 0) {
                    bool use_group =
                        (!ctrl && !shift && layout_editor_selection_count(engine) > 1 &&
                         layout_editor_is_selected(engine, hit.object_index) &&
                         hit.handle == HandleType::Center);
                    HitResult dispatch_hit = hit;
                    if (use_group) {
                        dispatch_hit.target = HitTarget::Group;
                        dispatch_hit.object_index = -1;
                    }
                    layout_editor_begin_drag(engine, *layout, dispatch_hit, mouse_x, mouse_y,
                                             viewport);
                }
            } else if (hit.target == HitTarget::Group) {
                if (layout_editor_selection_count(engine) > 1)
                    layout_editor_begin_drag(engine, *layout, hit, mouse_x, mouse_y, viewport);
            }
        } else {
            if (!shift && !ctrl)
                layout_editor_clear_selection(engine);
            layout_editor_end_drag(engine);
        }
    } else if (mouse_down && layout_editor_is_dragging(engine)) {
        if (layout_editor_update_drag(engine, *layout, mouse_x, mouse_y, state.snap_enabled,
                                      state.grid_step)) {
            state.layout_dirty = true;
            state.drag_dirty = true;
        }
    } else if (!mouse_down && state.mouse_was_down) {
        if (state.drag_dirty) {
            layout_editor_history_commit(engine, *layout);
            state.drag_dirty = false;
        }
        layout_editor_end_drag(engine);
    }

    state.mouse_was_down = mouse_down;
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
            if (save_ui_layout(*layout))
                append_status(engine, "Layout saved");
            else
                append_status(engine, "Failed to save layout");
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G))
        state.snap_enabled = !state.snap_enabled;
    if (ImGui::IsKeyPressed(ImGuiKey_Equal))
        state.grid_step = std::max(0.01f, state.grid_step - 0.01f);
    if (ImGui::IsKeyPressed(ImGuiKey_Minus))
        state.grid_step = std::min(0.5f, state.grid_step + 0.01f);

    handle_mouse_input(engine);

    UILayout* layout = selected_layout_mutable(engine);
    if (!layout)
        return;
    bool ctrl = io.KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (layout_editor_history_undo(engine, *layout)) {
            state.layout_dirty = true;
            state.object_label_index = -1;
            append_status(engine, "Undo");
        }
    } else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        if (layout_editor_history_redo(engine, *layout)) {
            state.layout_dirty = true;
            state.object_label_index = -1;
            append_status(engine, "Redo");
        }
    } else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        copy_selection_to_clipboard(engine, *layout);
    } else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        if (paste_clipboard(engine, *layout)) {
            state.layout_dirty = true;
            layout_editor_history_commit(engine, *layout);
        }
    }

    float nudge = io.KeyShift ? 0.05f : 0.01f;
    bool nudged = false;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
        if (translate_selection(engine, *layout, -nudge, 0.0f))
            nudged = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
        if (translate_selection(engine, *layout, nudge, 0.0f))
            nudged = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
        if (translate_selection(engine, *layout, 0.0f, -nudge))
            nudged = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
        if (translate_selection(engine, *layout, 0.0f, nudge))
            nudged = true;
    }
    if (nudged) {
        state.layout_dirty = true;
        layout_editor_history_commit(engine, *layout);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        if (delete_selection(engine, *layout)) {
            state.layout_dirty = true;
            layout_editor_history_commit(engine, *layout);
        }
    }
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
                            state.grid_step);
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
