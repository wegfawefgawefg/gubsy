#include "engine/layout_editor/layout_editor.hpp"
#include "engine/layout_editor/layout_editor_hooks.hpp"
#include "engine/layout_editor/layout_editor_history.hpp"
#include "engine/layout_editor/layout_editor_interaction.hpp"
#include "engine/layout_editor/layout_editor_internal.hpp"
#include "engine/layout_editor/layout_editor_panel.hpp"
#include "engine/layout_editor/layout_editor_overlay.hpp"

#include "engine/globals.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"
#include "engine/graphics.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <cfloat>
#include <limits>
#include <cstdio>
#include <string>
#include <vector>

namespace layout_editor_internal {

bool g_active = false;
int g_selected_layout = 0;
float g_grid_step = 0.2f;
bool g_snap_enabled = true;
std::string g_status_text;
float g_status_timer = 0.0f;
bool g_follow_active_layout = true;
bool g_mouse_was_down = false;
char g_object_label_buffer[128]{};
int g_object_label_index = -1;
bool g_drag_dirty = false;
bool g_history_initialized = false;
int g_history_layout_id = -1;
int g_history_layout_width = 0;
int g_history_layout_height = 0;
bool g_layout_dirty = false;

} // namespace layout_editor_internal

using namespace layout_editor_internal;

namespace layout_editor_internal {

bool has_layouts() {
    return es && !es->ui_layouts_pool.empty();
}

UILayout* selected_layout_mutable() {
    if (!has_layouts())
        return nullptr;
    g_selected_layout = std::clamp(g_selected_layout, 0,
                                   static_cast<int>(es->ui_layouts_pool.size()) - 1);
    return &es->ui_layouts_pool[static_cast<std::size_t>(g_selected_layout)];
}

const UILayout* selected_layout() {
    return selected_layout_mutable();
}

void append_status(const std::string& text) {
    g_status_text = text;
    g_status_timer = 3.0f;
}

} // namespace layout_editor_internal

namespace {

struct PendingLayoutRequest {
    bool valid{false};
    int id{-1};
    int width{0};
    int height{0};
};

PendingLayoutRequest g_last_request{};

struct Clipboard {
    std::vector<UIObject> objects;
};

Clipboard g_clipboard;
constexpr float kClipboardNudge = 0.02f;

void ensure_history_for_selection() {
    UILayout* layout = selected_layout_mutable();
    if (!layout) {
        g_history_initialized = false;
        g_history_layout_id = -1;
        g_history_layout_width = 0;
        g_history_layout_height = 0;
        return;
    }
    if (!g_history_initialized ||
        layout->id != g_history_layout_id ||
        layout->resolution_width != g_history_layout_width ||
        layout->resolution_height != g_history_layout_height) {
        layout_editor_history_reset(*layout);
        layout_editor_clear_selection();
        g_history_initialized = true;
        g_history_layout_id = layout->id;
        g_history_layout_width = layout->resolution_width;
        g_history_layout_height = layout->resolution_height;
        g_object_label_index = -1;
        g_object_label_buffer[0] = '\0';
    }
}

bool copy_selection_to_clipboard(const UILayout& layout) {
    const auto& sel = layout_editor_selection_indices();
    if (sel.empty())
        return false;
    g_clipboard.objects.clear();
    for (int index : sel) {
        if (index < 0 || index >= static_cast<int>(layout.objects.size()))
            continue;
        g_clipboard.objects.push_back(layout.objects[static_cast<std::size_t>(index)]);
    }
    return !g_clipboard.objects.empty();
}

bool paste_clipboard(UILayout& layout) {
    if (g_clipboard.objects.empty())
        return false;
    std::vector<int> new_indices;
    new_indices.reserve(g_clipboard.objects.size());
    for (const auto& obj : g_clipboard.objects) {
        UIObject copy = obj;
        copy.id = generate_ui_object_id();
        copy.x = std::clamp(copy.x + kClipboardNudge, 0.0f, 1.0f - copy.w);
        copy.y = std::clamp(copy.y + kClipboardNudge, 0.0f, 1.0f - copy.h);
        layout.objects.push_back(copy);
        new_indices.push_back(static_cast<int>(layout.objects.size()) - 1);
    }
    layout_editor_clear_selection();
    for (int idx : new_indices)
        layout_editor_add_to_selection(idx);
    return true;
}

bool delete_selection(UILayout& layout) {
    const auto& sel = layout_editor_selection_indices();
    if (sel.empty())
        return false;
    std::vector<int> to_remove = sel;
    std::sort(to_remove.begin(), to_remove.end());
    for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
        if (*it < 0 || *it >= static_cast<int>(layout.objects.size()))
            continue;
        layout.objects.erase(layout.objects.begin() + *it);
    }
    layout_editor_clear_selection();
    return true;
}

bool translate_selection(UILayout& layout, float dx, float dy) {
    const auto& sel = layout_editor_selection_indices();
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
        max_dx_positive = std::min(max_dx_positive, 1.0f - (obj.x + obj.w));
        max_dx_negative = std::min(max_dx_negative, obj.x);
        max_dy_positive = std::min(max_dy_positive, 1.0f - (obj.y + obj.h));
        max_dy_negative = std::min(max_dy_negative, obj.y);
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
        obj.x = std::clamp(obj.x + dx, 0.0f, 1.0f - obj.w);
        obj.y = std::clamp(obj.y + dy, 0.0f, 1.0f - obj.h);
    }
    return true;
}

bool select_layout_exact(int id, int width, int height) {
    if (!es)
        return false;
    for (std::size_t i = 0; i < es->ui_layouts_pool.size(); ++i) {
        const auto& layout = es->ui_layouts_pool[i];
        if (layout.id == id &&
            layout.resolution_width == width &&
            layout.resolution_height == height) {
            g_selected_layout = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

void auto_follow_selection() {
    if (!g_follow_active_layout)
        return;
    if (g_last_request.valid) {
        if (select_layout_exact(g_last_request.id,
                                g_last_request.width,
                                g_last_request.height))
            return;
    }
    if (!gg || !has_layouts())
        return;
    int target_w = static_cast<int>(gg->render_dims.x);
    int target_h = static_cast<int>(gg->render_dims.y);
    if (target_w <= 0 || target_h <= 0)
        return;
    int best_idx = g_selected_layout;
    int best_score = INT_MAX;
    for (std::size_t i = 0; i < es->ui_layouts_pool.size(); ++i) {
        const auto& layout = es->ui_layouts_pool[i];
        int dw = layout.resolution_width - target_w;
        int dh = layout.resolution_height - target_h;
        int score = dw * dw + dh * dh;
        if (score < best_score) {
            best_score = score;
            best_idx = static_cast<int>(i);
        }
    }
    g_selected_layout = best_idx;
}

void handle_mouse_input() {
    if (!g_active || !es)
        return;
    if (ImGui::GetCurrentContext()) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            g_mouse_was_down = io.MouseDown[0];
            return;
        }
    }
    LayoutEditorViewport viewport = layout_editor_get_viewport();
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return;
    UILayout* layout = selected_layout_mutable();
    if (!layout)
        return;

    float mouse_x = static_cast<float>(es->device_state.mouse_x);
    float mouse_y = static_cast<float>(es->device_state.mouse_y);
    bool mouse_down = (es->device_state.mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    bool shift = false;
    bool ctrl = false;
    if (ImGui::GetCurrentContext()) {
        const ImGuiIO& io = ImGui::GetIO();
        shift = io.KeyShift;
        ctrl = io.KeyCtrl;
    }

    if (mouse_down && !g_mouse_was_down) {
        g_drag_dirty = false;
        HitResult hit{};
        if (layout_editor_hit_test(*layout, viewport, mouse_x, mouse_y, hit)) {
            if (hit.target == HitTarget::Object && hit.object_index >= 0) {
                bool was_selected = layout_editor_is_selected(hit.object_index);
                int selection_count = layout_editor_selection_count();
                if (ctrl) {
                    if (was_selected)
                        layout_editor_remove_from_selection(hit.object_index);
                    else
                        layout_editor_add_to_selection(hit.object_index);
                } else if (shift) {
                    layout_editor_add_to_selection(hit.object_index);
                } else if (!was_selected || selection_count <= 1) {
                    layout_editor_select_single(hit.object_index);
                }
                selection_count = layout_editor_selection_count();

                if (layout_editor_selection_count() > 0) {
                    bool use_group = (!ctrl && !shift &&
                                      layout_editor_selection_count() > 1 &&
                                      layout_editor_is_selected(hit.object_index) &&
                                      hit.handle == HandleType::Center);
                    HitResult dispatch_hit = hit;
                    if (use_group) {
                        dispatch_hit.target = HitTarget::Group;
                        dispatch_hit.object_index = -1;
                    }
                    layout_editor_begin_drag(*layout, dispatch_hit, mouse_x, mouse_y, viewport);
                }
            } else if (hit.target == HitTarget::Group) {
                if (layout_editor_selection_count() > 1)
                    layout_editor_begin_drag(*layout, hit, mouse_x, mouse_y, viewport);
            }
        } else {
            if (!shift && !ctrl)
                layout_editor_clear_selection();
            layout_editor_end_drag();
        }
    } else if (mouse_down && layout_editor_is_dragging()) {
        if (layout_editor_update_drag(*layout, mouse_x, mouse_y, g_snap_enabled, g_grid_step)) {
            g_layout_dirty = true;
            g_drag_dirty = true;
        }
    } else if (!mouse_down && g_mouse_was_down) {
        if (g_drag_dirty) {
            layout_editor_history_commit(*layout);
            g_drag_dirty = false;
        }
        layout_editor_end_drag();
    }

    g_mouse_was_down = mouse_down;
}

void handle_hotkeys() {
    if (!ImGui::GetCurrentContext())
        return;
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_L) && io.KeyCtrl) {
        g_active = !g_active;
        if (!g_active)
            append_status("Layout editor deactivated");
        else
            append_status("Layout editor activated");
    }
    if (!g_active)
        return;
    if (has_layouts() && ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
        if (const UILayout* layout = selected_layout()) {
            if (save_ui_layout(*layout))
                append_status("Layout saved");
            else
                append_status("Failed to save layout");
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G))
        g_snap_enabled = !g_snap_enabled;
    if (ImGui::IsKeyPressed(ImGuiKey_Equal))
        g_grid_step = std::max(0.01f, g_grid_step - 0.01f);
    if (ImGui::IsKeyPressed(ImGuiKey_Minus))
        g_grid_step = std::min(0.5f, g_grid_step + 0.01f);

    handle_mouse_input();

    UILayout* layout = selected_layout_mutable();
    if (!layout)
        return;
    bool ctrl = io.KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (layout_editor_history_undo(*layout)) {
            g_layout_dirty = true;
            g_object_label_index = -1;
            append_status("Undo");
        }
    } else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        if (layout_editor_history_redo(*layout)) {
            g_layout_dirty = true;
            g_object_label_index = -1;
            append_status("Redo");
        }
    } else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
        copy_selection_to_clipboard(*layout);
    } else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        if (paste_clipboard(*layout)) {
            g_layout_dirty = true;
            layout_editor_history_commit(*layout);
        }
    }

    float nudge = io.KeyShift ? 0.05f : 0.01f;
    bool nudged = false;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
        if (translate_selection(*layout, -nudge, 0.0f))
            nudged = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
        if (translate_selection(*layout, nudge, 0.0f))
            nudged = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
        if (translate_selection(*layout, 0.0f, -nudge))
            nudged = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
        if (translate_selection(*layout, 0.0f, nudge))
            nudged = true;
    }
    if (nudged) {
        g_layout_dirty = true;
        layout_editor_history_commit(*layout);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        if (delete_selection(*layout)) {
            g_layout_dirty = true;
            layout_editor_history_commit(*layout);
        }
    }
}

} // namespace

void layout_editor_notify_active_layout(int layout_id,
                                        int resolution_width,
                                        int resolution_height) {
    g_last_request.valid = true;
    g_last_request.id = layout_id;
    g_last_request.width = resolution_width;
    g_last_request.height = resolution_height;
    if (g_follow_active_layout)
        auto_follow_selection();
}

bool layout_editor_consume_dirty_flag() {
    bool dirty = g_layout_dirty;
    g_layout_dirty = false;
    return dirty;
}

void layout_editor_begin_frame(float dt) {
    if (g_follow_active_layout)
        auto_follow_selection();
    ensure_history_for_selection();
    handle_hotkeys();
    if (g_active)
        layout_editor_render_panel(dt);
    else
        g_status_timer = std::max(0.0f, g_status_timer - dt);
}

bool layout_editor_is_active() {
    return g_active;
}

bool layout_editor_wants_input() {
    return g_active;
}

void layout_editor_render(SDL_Renderer* renderer,
                          int screen_width,
                          int screen_height,
                          float origin_x,
                          float origin_y) {
    if (!g_active || !renderer)
        return;
    if (!has_layouts())
        return;
    layout_editor_set_viewport(LayoutEditorViewport{origin_x,
                                                    origin_y,
                                                    static_cast<float>(screen_width),
                                                    static_cast<float>(screen_height)});
    layout_editor_draw_grid(renderer, screen_width, screen_height,
                            origin_x, origin_y, g_grid_step);
    if (const UILayout* layout = selected_layout()) {
        int dragging_idx = layout_editor_dragging_index();
        layout_editor_draw_layout(renderer, *layout, screen_width, screen_height,
                                  origin_x, origin_y, dragging_idx);
    }
}

void layout_editor_shutdown() {
    g_active = false;
    g_status_text.clear();
    g_status_timer = 0.0f;
    layout_editor_history_shutdown();
    g_history_initialized = false;
}
