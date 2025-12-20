#include "engine/layout_editor/layout_editor_interaction.hpp"

#include "engine/ui_layouts.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float kMinSize = 0.01f;
constexpr float kSnapTolBase = 0.01f;
constexpr float kSnapTolFactor = 0.15f;

struct DragState {
    bool active{false};
    int object_index{-1};
    HandleType handle{HandleType::Center};
    float offset_x{0.0f};
    float offset_y{0.0f};
    float start_x{0.0f};
    float start_y{0.0f};
    float start_w{0.0f};
    float start_h{0.0f};
};

struct HandleRect {
    HandleType type;
    SDL_FRect rect;
};

LayoutEditorViewport g_viewport{};
int g_selected_index = -1;
DragState g_drag{};

inline bool contains(const SDL_FRect& rect, float x, float y) {
    return x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

std::array<HandleRect, 9> build_handle_rects(const SDL_FRect& rect) {
    std::array<HandleRect, 9> handles;
    float half_corner = kHandleSize * 0.5f;
    float edge_len_v = std::min(rect.h, kEdgeHandleLength);
    float edge_len_h = std::min(rect.w, kEdgeHandleLength);

    handles[0] = {HandleType::CornerTopLeft,
                  SDL_FRect{rect.x - half_corner, rect.y - half_corner, kHandleSize, kHandleSize}};
    handles[1] = {HandleType::CornerTopRight,
                  SDL_FRect{rect.x + rect.w - half_corner, rect.y - half_corner, kHandleSize, kHandleSize}};
    handles[2] = {HandleType::CornerBottomLeft,
                  SDL_FRect{rect.x - half_corner, rect.y + rect.h - half_corner, kHandleSize, kHandleSize}};
    handles[3] = {HandleType::CornerBottomRight,
                  SDL_FRect{rect.x + rect.w - half_corner, rect.y + rect.h - half_corner, kHandleSize, kHandleSize}};

    handles[4] = {HandleType::EdgeTop,
                  SDL_FRect{rect.x + rect.w * 0.5f - edge_len_h * 0.5f,
                            rect.y - kEdgeHandleThickness * 0.5f,
                            edge_len_h,
                            kEdgeHandleThickness}};
    handles[5] = {HandleType::EdgeBottom,
                  SDL_FRect{rect.x + rect.w * 0.5f - edge_len_h * 0.5f,
                            rect.y + rect.h - kEdgeHandleThickness * 0.5f,
                            edge_len_h,
                            kEdgeHandleThickness}};
    handles[6] = {HandleType::EdgeLeft,
                  SDL_FRect{rect.x - kEdgeHandleThickness * 0.5f,
                            rect.y + rect.h * 0.5f - edge_len_v * 0.5f,
                            kEdgeHandleThickness,
                            edge_len_v}};
    handles[7] = {HandleType::EdgeRight,
                  SDL_FRect{rect.x + rect.w - kEdgeHandleThickness * 0.5f,
                            rect.y + rect.h * 0.5f - edge_len_v * 0.5f,
                            kEdgeHandleThickness,
                            edge_len_v}};

    handles[8] = {HandleType::Center, rect};
    return handles;
}

float snap_value(float value, float step) {
    if (step <= 0.0f)
        return value;
    return std::round(value / step) * step;
}

float snap_single_edge(float value, float step) {
    if (step <= 0.0f)
        return value;
    float snapped = snap_value(value, step);
    float tolerance = std::max(step * kSnapTolFactor, kSnapTolBase);
    return (std::fabs(snapped - value) <= tolerance) ? snapped : value;
}

float snap_pair(float start, float size, float step) {
    if (step <= 0.0f)
        return start;
    float tolerance = std::max(step * kSnapTolFactor, kSnapTolBase);
    float left_candidate = snap_value(start, step);
    float right_candidate = snap_value(start + size, step) - size;
    left_candidate = std::clamp(left_candidate, 0.0f, 1.0f - size);
    right_candidate = std::clamp(right_candidate, 0.0f, 1.0f - size);
    float best = start;
    float best_diff = tolerance + 1.0f;
    float diff_left = std::fabs(left_candidate - start);
    if (diff_left <= tolerance && diff_left < best_diff) {
        best = left_candidate;
        best_diff = diff_left;
    }
    float diff_right = std::fabs(right_candidate - start);
    if (diff_right <= tolerance && diff_right < best_diff)
        best = right_candidate;
    return best;
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
                            HitResult& out_hit) {
    out_hit = HitResult{};
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return false;
    if (!contains(SDL_FRect{viewport.origin_x, viewport.origin_y, viewport.width, viewport.height},
                  mouse_x, mouse_y))
        return false;
    for (int i = static_cast<int>(layout.objects.size()) - 1; i >= 0; --i) {
        const auto& obj = layout.objects[static_cast<std::size_t>(i)];
        SDL_FRect rect{
            viewport.origin_x + obj.x * viewport.width,
            viewport.origin_y + obj.y * viewport.height,
            obj.w * viewport.width,
            obj.h * viewport.height};
        auto handles = build_handle_rects(rect);
        for (std::size_t h = 0; h < 8; ++h) { // exclude center for now
            if (contains(handles[h].rect, mouse_x, mouse_y)) {
                out_hit.object_index = i;
                out_hit.handle = handles[h].type;
                return true;
            }
        }
        if (contains(rect, mouse_x, mouse_y)) {
            out_hit.object_index = i;
            out_hit.handle = HandleType::Center;
            return true;
        }
    }
    return false;
}

void layout_editor_begin_drag(const UILayout& layout,
                              const HitResult& hit,
                              float mouse_x,
                              float mouse_y,
                              const LayoutEditorViewport& viewport) {
    g_drag = DragState{};
    if (hit.object_index < 0 || hit.object_index >= static_cast<int>(layout.objects.size()))
        return;
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return;
    float local_x = (mouse_x - viewport.origin_x) / viewport.width;
    float local_y = (mouse_y - viewport.origin_y) / viewport.height;
    const auto& obj = layout.objects[static_cast<std::size_t>(hit.object_index)];
    g_drag.active = true;
    g_drag.object_index = hit.object_index;
    g_drag.handle = hit.handle;
    g_drag.start_x = obj.x;
    g_drag.start_y = obj.y;
    g_drag.start_w = obj.w;
    g_drag.start_h = obj.h;
    g_drag.offset_x = local_x - obj.x;
    g_drag.offset_y = local_y - obj.y;
}

bool layout_editor_update_drag(UILayout& layout,
                               float mouse_x,
                               float mouse_y,
                               bool snap_enabled,
                               float grid_step) {
    if (!g_drag.active)
        return false;
    if (g_drag.object_index < 0 ||
        g_drag.object_index >= static_cast<int>(layout.objects.size())) {
        layout_editor_end_drag();
        return false;
    }
    if (g_viewport.width <= 0.0f || g_viewport.height <= 0.0f)
        return false;

    auto& obj = layout.objects[static_cast<std::size_t>(g_drag.object_index)];
    float local_x = (mouse_x - g_viewport.origin_x) / g_viewport.width;
    float local_y = (mouse_y - g_viewport.origin_y) / g_viewport.height;
    bool changed = false;

    auto apply_snap_pos = [&](float start, float size) {
        return snap_enabled ? snap_pair(start, size, grid_step) : start;
    };
    auto snap_single = [&](float coord) {
        return snap_enabled ? snap_single_edge(coord, grid_step) : coord;
    };

    switch (g_drag.handle) {
        case HandleType::Center: {
            float new_x = local_x - g_drag.offset_x;
            float new_y = local_y - g_drag.offset_y;
            new_x = std::clamp(new_x, 0.0f, 1.0f - obj.w);
            new_y = std::clamp(new_y, 0.0f, 1.0f - obj.h);
            if (snap_enabled && grid_step > 0.0f) {
                new_x = apply_snap_pos(new_x, obj.w);
                new_y = apply_snap_pos(new_y, obj.h);
            }
            changed = (std::fabs(obj.x - new_x) > 1e-5f) ||
                      (std::fabs(obj.y - new_y) > 1e-5f);
            obj.x = new_x;
            obj.y = new_y;
            break;
        }
        case HandleType::EdgeLeft: {
            float anchor = g_drag.start_x + g_drag.start_w;
            float new_left = std::clamp(local_x, 0.0f, anchor - kMinSize);
            if (snap_enabled)
                new_left = snap_single(new_left);
            float new_width = anchor - new_left;
            changed = (std::fabs(obj.x - new_left) > 1e-5f) ||
                      (std::fabs(obj.w - new_width) > 1e-5f);
            obj.x = new_left;
            obj.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.x));
            break;
        }
        case HandleType::EdgeRight: {
            float anchor = g_drag.start_x;
            float new_right = std::clamp(local_x, anchor + kMinSize, 1.0f);
            if (snap_enabled)
                new_right = snap_single(new_right);
            float new_width = new_right - anchor;
            changed = (std::fabs(obj.w - new_width) > 1e-5f);
            obj.x = anchor;
            obj.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.x));
            break;
        }
        case HandleType::EdgeTop: {
            float anchor = g_drag.start_y + g_drag.start_h;
            float new_top = std::clamp(local_y, 0.0f, anchor - kMinSize);
            if (snap_enabled)
                new_top = snap_single(new_top);
            float new_height = anchor - new_top;
            changed = (std::fabs(obj.y - new_top) > 1e-5f) ||
                      (std::fabs(obj.h - new_height) > 1e-5f);
            obj.y = new_top;
            obj.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.y));
            break;
        }
        case HandleType::EdgeBottom: {
            float anchor = g_drag.start_y;
            float new_bottom = std::clamp(local_y, anchor + kMinSize, 1.0f);
            if (snap_enabled)
                new_bottom = snap_single(new_bottom);
            float new_height = new_bottom - anchor;
            changed = (std::fabs(obj.h - new_height) > 1e-5f);
            obj.y = anchor;
            obj.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.y));
            break;
        }
        case HandleType::CornerTopLeft: {
            float anchor_right = g_drag.start_x + g_drag.start_w;
            float anchor_bottom = g_drag.start_y + g_drag.start_h;
            float new_left = std::clamp(local_x, 0.0f, anchor_right - kMinSize);
            float new_top = std::clamp(local_y, 0.0f, anchor_bottom - kMinSize);
            if (snap_enabled) {
                new_left = snap_single(new_left);
                new_top = snap_single(new_top);
            }
            float new_width = anchor_right - new_left;
            float new_height = anchor_bottom - new_top;
            changed = (std::fabs(obj.x - new_left) > 1e-5f) ||
                      (std::fabs(obj.y - new_top) > 1e-5f) ||
                      (std::fabs(obj.w - new_width) > 1e-5f) ||
                      (std::fabs(obj.h - new_height) > 1e-5f);
            obj.x = new_left;
            obj.y = new_top;
            obj.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.x));
            obj.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.y));
            break;
        }
        case HandleType::CornerTopRight: {
            float anchor_left = g_drag.start_x;
            float anchor_bottom = g_drag.start_y + g_drag.start_h;
            float new_right = std::clamp(local_x, anchor_left + kMinSize, 1.0f);
            float new_top = std::clamp(local_y, 0.0f, anchor_bottom - kMinSize);
            if (snap_enabled) {
                new_right = snap_single(new_right);
                new_top = snap_single(new_top);
            }
            float new_width = new_right - anchor_left;
            float new_height = anchor_bottom - new_top;
            changed = (std::fabs(obj.w - new_width) > 1e-5f) ||
                      (std::fabs(obj.y - new_top) > 1e-5f) ||
                      (std::fabs(obj.h - new_height) > 1e-5f);
            obj.x = anchor_left;
            obj.y = new_top;
            obj.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.x));
            obj.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.y));
            break;
        }
        case HandleType::CornerBottomLeft: {
            float anchor_right = g_drag.start_x + g_drag.start_w;
            float anchor_top = g_drag.start_y;
            float new_left = std::clamp(local_x, 0.0f, anchor_right - kMinSize);
            float new_bottom = std::clamp(local_y, anchor_top + kMinSize, 1.0f);
            if (snap_enabled) {
                new_left = snap_single(new_left);
                new_bottom = snap_single(new_bottom);
            }
            float new_width = anchor_right - new_left;
            float new_height = new_bottom - anchor_top;
            changed = (std::fabs(obj.x - new_left) > 1e-5f) ||
                      (std::fabs(obj.h - new_height) > 1e-5f) ||
                      (std::fabs(obj.w - new_width) > 1e-5f);
            obj.x = new_left;
            obj.y = anchor_top;
            obj.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.x));
            obj.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.y));
            break;
        }
        case HandleType::CornerBottomRight: {
            float anchor_left = g_drag.start_x;
            float anchor_top = g_drag.start_y;
            float new_right = std::clamp(local_x, anchor_left + kMinSize, 1.0f);
            float new_bottom = std::clamp(local_y, anchor_top + kMinSize, 1.0f);
            if (snap_enabled) {
                new_right = snap_single(new_right);
                new_bottom = snap_single(new_bottom);
            }
            float new_width = new_right - anchor_left;
            float new_height = new_bottom - anchor_top;
            changed = (std::fabs(obj.w - new_width) > 1e-5f) ||
                      (std::fabs(obj.h - new_height) > 1e-5f);
            obj.x = anchor_left;
            obj.y = anchor_top;
            obj.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.x));
            obj.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.y));
            break;
        }
    }
    return changed;
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

HandleType layout_editor_drag_handle() {
    return g_drag.handle;
}
