#include "engine/layout_editor/layout_editor_interaction.hpp"

#include "engine/ui_layouts.hpp"

#include <algorithm>
#include <cmath>

namespace {
LayoutEditorViewport g_viewport{};
int g_selected_index = -1;

struct DragState {
    bool active{false};
    int object_index{-1};
    float offset_x{0.0f};
    float offset_y{0.0f};
};

DragState g_drag{};

inline bool viewport_contains(const LayoutEditorViewport& vp, float x, float y) {
    return x >= vp.origin_x && y >= vp.origin_y &&
           x <= vp.origin_x + vp.width &&
           y <= vp.origin_y + vp.height;
}

inline float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

void layout_editor_set_viewport(const LayoutEditorViewport& viewport) {
    g_viewport = viewport;
}

LayoutEditorViewport layout_editor_get_viewport() {
    return g_viewport;
}

int layout_editor_selected_index() {
    return g_selected_index;
}

void layout_editor_select(int index) {
    g_selected_index = index;
}

void layout_editor_clear_selection() {
    g_selected_index = -1;
}

bool layout_editor_hit_test(const UILayout& layout,
                            const LayoutEditorViewport& viewport,
                            float mouse_x,
                            float mouse_y,
                            int& out_index) {
    out_index = -1;
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return false;
    if (!viewport_contains(viewport, mouse_x, mouse_y))
        return false;
    for (int i = static_cast<int>(layout.objects.size()) - 1; i >= 0; --i) {
        const auto& obj = layout.objects[static_cast<std::size_t>(i)];
        SDL_FRect rect{
            viewport.origin_x + obj.x * viewport.width,
            viewport.origin_y + obj.y * viewport.height,
            obj.w * viewport.width,
            obj.h * viewport.height};
        if (mouse_x >= rect.x && mouse_x <= rect.x + rect.w &&
            mouse_y >= rect.y && mouse_y <= rect.y + rect.h) {
            out_index = i;
            return true;
        }
    }
    return false;
}

void layout_editor_begin_drag(const UILayout& layout,
                              int object_index,
                              float mouse_x,
                              float mouse_y,
                              const LayoutEditorViewport& viewport) {
    g_drag = DragState{};
    if (object_index < 0 || object_index >= static_cast<int>(layout.objects.size()))
        return;
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return;
    float local_x = (mouse_x - viewport.origin_x) / viewport.width;
    float local_y = (mouse_y - viewport.origin_y) / viewport.height;
    const auto& obj = layout.objects[static_cast<std::size_t>(object_index)];
    g_drag.active = true;
    g_drag.object_index = object_index;
    g_drag.offset_x = local_x - obj.x;
    g_drag.offset_y = local_y - obj.y;
}

void layout_editor_update_drag(UILayout& layout,
                               float mouse_x,
                               float mouse_y,
                               bool snap_enabled,
                               float grid_step) {
    if (!g_drag.active)
        return;
    if (g_drag.object_index < 0 ||
        g_drag.object_index >= static_cast<int>(layout.objects.size())) {
        layout_editor_end_drag();
        return;
    }
    if (g_viewport.width <= 0.0f || g_viewport.height <= 0.0f)
        return;
    float local_x = (mouse_x - g_viewport.origin_x) / g_viewport.width;
    float local_y = (mouse_y - g_viewport.origin_y) / g_viewport.height;
    auto& obj = layout.objects[static_cast<std::size_t>(g_drag.object_index)];
    float new_x = local_x - g_drag.offset_x;
    float new_y = local_y - g_drag.offset_y;
    if (snap_enabled && grid_step > 0.0f) {
        new_x = std::round(new_x / grid_step) * grid_step;
        new_y = std::round(new_y / grid_step) * grid_step;
    }
    new_x = clamp01(new_x);
    new_y = clamp01(new_y);
    new_x = std::clamp(new_x, 0.0f, 1.0f - obj.w);
    new_y = std::clamp(new_y, 0.0f, 1.0f - obj.h);
    obj.x = new_x;
    obj.y = new_y;
}

void layout_editor_end_drag() {
    g_drag = DragState{};
}

bool layout_editor_is_dragging() {
    return g_drag.active;
}

int layout_editor_dragging_index() {
    return g_drag.active ? g_drag.object_index : -1;
}
