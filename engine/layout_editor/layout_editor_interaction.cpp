#include "engine/layout_editor/layout_editor_interaction.hpp"

#include "engine/engine_state.hpp"
#include "engine/layout_editor/layout_editor_internal.hpp"
#include "engine/ui_layouts.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

using layout_editor_internal::DragState;
using layout_editor_internal::editor_state;
using layout_editor_internal::GroupMember;
using layout_editor_internal::LayoutEditorState;

constexpr float kMinSize = 0.01f;
constexpr float kSnapTolBase = 0.01f;
constexpr float kSnapTolFactor = 0.075f;

struct HandleRect {
    HandleType type;
    SDL_FRect rect;
};

inline bool contains(const SDL_FRect& rect, float x, float y) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

std::array<HandleRect, 9> build_handle_rects(const SDL_FRect& rect) {
    std::array<HandleRect, 9> handles;
    float half_corner = kHandleSize * 0.5f;
    float edge_len_v = std::min(rect.h, kEdgeHandleLength);
    float edge_len_h = std::min(rect.w, kEdgeHandleLength);

    handles[0] = {HandleType::CornerTopLeft,
                  SDL_FRect{rect.x - half_corner, rect.y - half_corner, kHandleSize, kHandleSize}};
    handles[1] = {
        HandleType::CornerTopRight,
        SDL_FRect{rect.x + rect.w - half_corner, rect.y - half_corner, kHandleSize, kHandleSize}};
    handles[2] = {
        HandleType::CornerBottomLeft,
        SDL_FRect{rect.x - half_corner, rect.y + rect.h - half_corner, kHandleSize, kHandleSize}};
    handles[3] = {HandleType::CornerBottomRight,
                  SDL_FRect{rect.x + rect.w - half_corner, rect.y + rect.h - half_corner,
                            kHandleSize, kHandleSize}};

    handles[4] = {HandleType::EdgeTop, SDL_FRect{rect.x + rect.w * 0.5f - edge_len_h * 0.5f,
                                                 rect.y - kEdgeHandleThickness * 0.5f, edge_len_h,
                                                 kEdgeHandleThickness}};
    handles[5] = {HandleType::EdgeBottom, SDL_FRect{rect.x + rect.w * 0.5f - edge_len_h * 0.5f,
                                                    rect.y + rect.h - kEdgeHandleThickness * 0.5f,
                                                    edge_len_h, kEdgeHandleThickness}};
    handles[6] = {HandleType::EdgeLeft, SDL_FRect{rect.x - kEdgeHandleThickness * 0.5f,
                                                  rect.y + rect.h * 0.5f - edge_len_v * 0.5f,
                                                  kEdgeHandleThickness, edge_len_v}};
    handles[7] = {HandleType::EdgeRight, SDL_FRect{rect.x + rect.w - kEdgeHandleThickness * 0.5f,
                                                   rect.y + rect.h * 0.5f - edge_len_v * 0.5f,
                                                   kEdgeHandleThickness, edge_len_v}};

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

void sync_primary(LayoutEditorState& state) {
    if (state.selection.empty()) {
        state.primary = -1;
        return;
    }
    if (std::find(state.selection.begin(), state.selection.end(), state.primary) ==
        state.selection.end()) {
        state.primary = state.selection.back();
    }
}

float snap_value_to_edges(float value, const std::vector<float>& edges, float tolerance,
                          bool& snapped) {
    float best = value;
    float best_diff = tolerance + 1.0f;
    snapped = false;
    for (float edge : edges) {
        float diff = std::fabs(edge - value);
        if (diff <= tolerance && diff < best_diff) {
            best = edge;
            best_diff = diff;
            snapped = true;
        }
    }
    return best;
}

} // namespace

void layout_editor_set_viewport(EngineState& engine, const LayoutEditorViewport& viewport) {
    editor_state(engine).viewport = viewport;
}

LayoutEditorViewport layout_editor_get_viewport(const EngineState& engine) {
    return editor_state(engine).viewport;
}

const std::vector<int>& layout_editor_selection_indices(const EngineState& engine) {
    return editor_state(engine).selection;
}

int layout_editor_selection_count(const EngineState& engine) {
    return static_cast<int>(editor_state(engine).selection.size());
}

bool layout_editor_is_selected(const EngineState& engine, int index) {
    const auto& selection = editor_state(engine).selection;
    return std::find(selection.begin(), selection.end(), index) != selection.end();
}

int layout_editor_primary_selection(const EngineState& engine) {
    return editor_state(engine).primary;
}

void layout_editor_select_single(EngineState& engine, int index) {
    LayoutEditorState& state = editor_state(engine);
    state.selection.clear();
    if (index >= 0)
        state.selection.push_back(index);
    state.primary = index;
}

void layout_editor_add_to_selection(EngineState& engine, int index) {
    LayoutEditorState& state = editor_state(engine);
    if (index < 0)
        return;
    if (!layout_editor_is_selected(engine, index)) {
        state.selection.push_back(index);
        std::sort(state.selection.begin(), state.selection.end());
        state.primary = index;
    }
}

void layout_editor_remove_from_selection(EngineState& engine, int index) {
    LayoutEditorState& state = editor_state(engine);
    auto it = std::find(state.selection.begin(), state.selection.end(), index);
    if (it != state.selection.end()) {
        state.selection.erase(it);
        sync_primary(state);
    }
}

void layout_editor_clear_selection(EngineState& engine) {
    LayoutEditorState& state = editor_state(engine);
    state.selection.clear();
    state.primary = -1;
}

bool layout_editor_selection_bounds(const EngineState& engine, const UILayout& layout, float& min_x,
                                    float& min_y, float& max_x, float& max_y) {
    const auto& selection = editor_state(engine).selection;
    bool initialized = false;
    for (int index : selection) {
        if (index < 0 || index >= static_cast<int>(layout.objects.size()))
            continue;
        const auto& obj = layout.objects[static_cast<std::size_t>(index)];
        float obj_min_x = obj.rect.x;
        float obj_min_y = obj.rect.y;
        float obj_max_x = obj.rect.x + obj.rect.w;
        float obj_max_y = obj.rect.y + obj.rect.h;
        if (!initialized) {
            min_x = obj_min_x;
            min_y = obj_min_y;
            max_x = obj_max_x;
            max_y = obj_max_y;
            initialized = true;
        } else {
            min_x = std::min(min_x, obj_min_x);
            min_y = std::min(min_y, obj_min_y);
            max_x = std::max(max_x, obj_max_x);
            max_y = std::max(max_y, obj_max_y);
        }
    }
    return initialized;
}

bool layout_editor_hit_test(const EngineState& engine, const UILayout& layout,
                            const LayoutEditorViewport& viewport, float mouse_x, float mouse_y,
                            HitResult& out_hit) {
    out_hit = HitResult{};
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return false;
    if (!contains(SDL_FRect{viewport.origin_x, viewport.origin_y, viewport.width, viewport.height},
                  mouse_x, mouse_y)) {
        return false;
    }
    bool group_bounds_valid = false;
    SDL_FRect group_rect{};
    if (layout_editor_selection_count(engine) > 1) {
        float min_x = 0.0f;
        float min_y = 0.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        if (layout_editor_selection_bounds(engine, layout, min_x, min_y, max_x, max_y)) {
            group_bounds_valid = true;
            group_rect.x = viewport.origin_x + min_x * viewport.width;
            group_rect.y = viewport.origin_y + min_y * viewport.height;
            group_rect.w = std::max(0.0f, (max_x - min_x) * viewport.width);
            group_rect.h = std::max(0.0f, (max_y - min_y) * viewport.height);
            auto group_handles = build_handle_rects(group_rect);
            for (std::size_t h = 0; h < 8; ++h) {
                if (contains(group_handles[h].rect, mouse_x, mouse_y)) {
                    out_hit.target = HitTarget::Group;
                    out_hit.handle = group_handles[h].type;
                    return true;
                }
            }
        }
    }
    for (int i = static_cast<int>(layout.objects.size()) - 1; i >= 0; --i) {
        const auto& obj = layout.objects[static_cast<std::size_t>(i)];
        SDL_FRect rect{viewport.origin_x + obj.rect.x * viewport.width,
                       viewport.origin_y + obj.rect.y * viewport.height,
                       obj.rect.w * viewport.width, obj.rect.h * viewport.height};
        auto handles = build_handle_rects(rect);
        for (std::size_t h = 0; h < 8; ++h) {
            if (contains(handles[h].rect, mouse_x, mouse_y)) {
                out_hit.target = HitTarget::Object;
                out_hit.object_index = i;
                out_hit.handle = handles[h].type;
                return true;
            }
        }
        if (contains(rect, mouse_x, mouse_y)) {
            out_hit.target = HitTarget::Object;
            out_hit.object_index = i;
            out_hit.handle = HandleType::Center;
            return true;
        }
    }
    if (group_bounds_valid && contains(group_rect, mouse_x, mouse_y)) {
        out_hit.target = HitTarget::Group;
        out_hit.handle = HandleType::Center;
        return true;
    }
    return false;
}

void layout_editor_begin_drag(EngineState& engine, const UILayout& layout, const HitResult& hit,
                              float mouse_x, float mouse_y, const LayoutEditorViewport& viewport) {
    LayoutEditorState& state = editor_state(engine);
    DragState& drag = state.drag;
    drag = DragState{};
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return;
    float local_x = (mouse_x - viewport.origin_x) / viewport.width;
    float local_y = (mouse_y - viewport.origin_y) / viewport.height;
    drag.handle = hit.handle;
    if (hit.target == HitTarget::Group) {
        if (layout_editor_selection_count(engine) < 1)
            return;
        float min_x = 0.0f;
        float min_y = 0.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        if (!layout_editor_selection_bounds(engine, layout, min_x, min_y, max_x, max_y))
            return;
        drag.active = true;
        drag.group = true;
        drag.group_start_x = min_x;
        drag.group_start_y = min_y;
        drag.group_start_w = std::max(kMinSize, max_x - min_x);
        drag.group_start_h = std::max(kMinSize, max_y - min_y);
        drag.offset_x = local_x - drag.group_start_x;
        drag.offset_y = local_y - drag.group_start_y;
        drag.members.clear();
        drag.snap_edges_x.clear();
        drag.snap_edges_y.clear();
        for (int i = 0; i < static_cast<int>(layout.objects.size()); ++i) {
            if (layout_editor_is_selected(engine, i))
                continue;
            const auto& other = layout.objects[static_cast<std::size_t>(i)];
            drag.snap_edges_x.push_back(other.rect.x);
            drag.snap_edges_x.push_back(other.rect.x + other.rect.w);
            drag.snap_edges_y.push_back(other.rect.y);
            drag.snap_edges_y.push_back(other.rect.y + other.rect.h);
        }
        const float inv_width = (drag.group_start_w > 1e-6f) ? 1.0f / drag.group_start_w : 0.0f;
        const float inv_height = (drag.group_start_h > 1e-6f) ? 1.0f / drag.group_start_h : 0.0f;
        for (int index : layout_editor_selection_indices(engine)) {
            if (index < 0 || index >= static_cast<int>(layout.objects.size()))
                continue;
            const auto& obj = layout.objects[static_cast<std::size_t>(index)];
            GroupMember member;
            member.index = index;
            member.start_x = obj.rect.x;
            member.start_y = obj.rect.y;
            member.start_w = obj.rect.w;
            member.start_h = obj.rect.h;
            member.rel_x = inv_width > 0.0f ? (obj.rect.x - drag.group_start_x) * inv_width : 0.0f;
            member.rel_y =
                inv_height > 0.0f ? (obj.rect.y - drag.group_start_y) * inv_height : 0.0f;
            member.rel_w = inv_width > 0.0f ? obj.rect.w * inv_width : 0.0f;
            member.rel_h = inv_height > 0.0f ? obj.rect.h * inv_height : 0.0f;
            drag.members.push_back(member);
        }
        if (drag.members.empty())
            drag = DragState{};
        return;
    }
    if (hit.object_index < 0 || hit.object_index >= static_cast<int>(layout.objects.size()))
        return;
    const auto& obj = layout.objects[static_cast<std::size_t>(hit.object_index)];
    drag.active = true;
    drag.object_index = hit.object_index;
    drag.group = false;
    drag.start_x = obj.rect.x;
    drag.start_y = obj.rect.y;
    drag.start_w = obj.rect.w;
    drag.start_h = obj.rect.h;
    drag.offset_x = local_x - obj.rect.x;
    drag.offset_y = local_y - obj.rect.y;
    drag.snap_edges_x.clear();
    drag.snap_edges_y.clear();
    for (int i = 0; i < static_cast<int>(layout.objects.size()); ++i) {
        if (i == hit.object_index)
            continue;
        const auto& other = layout.objects[static_cast<std::size_t>(i)];
        drag.snap_edges_x.push_back(other.rect.x);
        drag.snap_edges_x.push_back(other.rect.x + other.rect.w);
        drag.snap_edges_y.push_back(other.rect.y);
        drag.snap_edges_y.push_back(other.rect.y + other.rect.h);
    }
}

bool layout_editor_update_drag(EngineState& engine, UILayout& layout, float mouse_x, float mouse_y,
                               bool snap_enabled, float grid_step) {
    LayoutEditorState& state = editor_state(engine);
    DragState& drag = state.drag;
    if (!drag.active)
        return false;
    if (state.viewport.width <= 0.0f || state.viewport.height <= 0.0f)
        return false;

    float local_x = (mouse_x - state.viewport.origin_x) / state.viewport.width;
    float local_y = (mouse_y - state.viewport.origin_y) / state.viewport.height;

    auto apply_snap_pos = [&](float start, float size) {
        return snap_enabled ? snap_pair(start, size, grid_step) : start;
    };
    auto snap_single = [&](float coord) {
        return snap_enabled ? snap_single_edge(coord, grid_step) : coord;
    };
    float edge_tolerance = std::max(grid_step * kSnapTolFactor, kSnapTolBase);
    auto snap_axis = [&](float& start, float size, const std::vector<float>& edges) {
        if (!snap_enabled || edges.empty())
            return;
        bool snapped_left = false;
        float new_start = snap_value_to_edges(start, edges, edge_tolerance, snapped_left);
        if (snapped_left)
            start = new_start;
        bool snapped_right = false;
        float end = start + size;
        float snapped_end = snap_value_to_edges(end, edges, edge_tolerance, snapped_right);
        if (snapped_right)
            start = snapped_end - size;
    };
    auto snap_edge_value = [&](float& coord, const std::vector<float>& edges) {
        if (!snap_enabled || edges.empty())
            return;
        bool snapped = false;
        float new_coord = snap_value_to_edges(coord, edges, edge_tolerance, snapped);
        if (snapped)
            coord = new_coord;
    };

    if (drag.group) {
        if (drag.members.empty())
            return false;
        bool changed = false;
        float new_left = drag.group_start_x;
        float new_top = drag.group_start_y;
        float new_width = drag.group_start_w;
        float new_height = drag.group_start_h;

        auto apply_member_changes = [&](float target_left, float target_top, float target_width,
                                        float target_height) {
            const float epsilon = 1e-5f;
            for (const auto& member : drag.members) {
                if (member.index < 0 || member.index >= static_cast<int>(layout.objects.size()))
                    continue;
                auto& obj = layout.objects[static_cast<std::size_t>(member.index)];
                float new_w = std::max(kMinSize, member.rel_w * target_width);
                float new_h = std::max(kMinSize, member.rel_h * target_height);
                float new_x = target_left + member.rel_x * target_width;
                float new_y = target_top + member.rel_y * target_height;
                new_x = std::clamp(new_x, 0.0f, 1.0f - new_w);
                new_y = std::clamp(new_y, 0.0f, 1.0f - new_h);

                if (std::fabs(obj.rect.x - new_x) > epsilon ||
                    std::fabs(obj.rect.y - new_y) > epsilon ||
                    std::fabs(obj.rect.w - new_w) > epsilon ||
                    std::fabs(obj.rect.h - new_h) > epsilon) {
                    changed = true;
                }
                obj.rect.x = new_x;
                obj.rect.y = new_y;
                obj.rect.w = new_w;
                obj.rect.h = new_h;
            }
        };

        switch (drag.handle) {
        case HandleType::Center: {
            new_left = local_x - drag.offset_x;
            new_top = local_y - drag.offset_y;
            new_left = std::clamp(new_left, 0.0f, 1.0f - new_width);
            new_top = std::clamp(new_top, 0.0f, 1.0f - new_height);
            if (snap_enabled && grid_step > 0.0f) {
                new_left = apply_snap_pos(new_left, new_width);
                new_top = apply_snap_pos(new_top, new_height);
            }
            snap_axis(new_left, new_width, drag.snap_edges_x);
            snap_axis(new_top, new_height, drag.snap_edges_y);
            float delta_x = new_left - drag.group_start_x;
            float delta_y = new_top - drag.group_start_y;
            const float epsilon = 1e-5f;
            for (const auto& member : drag.members) {
                if (member.index < 0 || member.index >= static_cast<int>(layout.objects.size())) {
                    continue;
                }
                auto& obj = layout.objects[static_cast<std::size_t>(member.index)];
                float new_x = std::clamp(member.start_x + delta_x, 0.0f, 1.0f - obj.rect.w);
                float new_y = std::clamp(member.start_y + delta_y, 0.0f, 1.0f - obj.rect.h);
                if (std::fabs(obj.rect.x - new_x) > epsilon ||
                    std::fabs(obj.rect.y - new_y) > epsilon) {
                    changed = true;
                }
                obj.rect.x = new_x;
                obj.rect.y = new_y;
            }
            return changed;
        }
        case HandleType::EdgeLeft: {
            float anchor = drag.group_start_x + drag.group_start_w;
            new_left = std::clamp(local_x, 0.0f, anchor - kMinSize);
            if (snap_enabled)
                new_left = snap_single(new_left);
            snap_edge_value(new_left, drag.snap_edges_x);
            new_width = std::max(kMinSize, anchor - new_left);
            apply_member_changes(new_left, drag.group_start_y, new_width, new_height);
            return changed;
        }
        case HandleType::EdgeRight: {
            float anchor = drag.group_start_x;
            float new_right = std::clamp(local_x, anchor + kMinSize, 1.0f);
            if (snap_enabled)
                new_right = snap_single(new_right);
            snap_edge_value(new_right, drag.snap_edges_x);
            new_width = std::max(kMinSize, new_right - anchor);
            apply_member_changes(anchor, drag.group_start_y, new_width, new_height);
            return changed;
        }
        case HandleType::EdgeTop: {
            float anchor = drag.group_start_y + drag.group_start_h;
            new_top = std::clamp(local_y, 0.0f, anchor - kMinSize);
            if (snap_enabled)
                new_top = snap_single(new_top);
            snap_edge_value(new_top, drag.snap_edges_y);
            new_height = std::max(kMinSize, anchor - new_top);
            apply_member_changes(drag.group_start_x, new_top, new_width, new_height);
            return changed;
        }
        case HandleType::EdgeBottom: {
            float anchor = drag.group_start_y;
            float new_bottom = std::clamp(local_y, anchor + kMinSize, 1.0f);
            if (snap_enabled)
                new_bottom = snap_single(new_bottom);
            snap_edge_value(new_bottom, drag.snap_edges_y);
            new_height = std::max(kMinSize, new_bottom - anchor);
            apply_member_changes(drag.group_start_x, anchor, new_width, new_height);
            return changed;
        }
        case HandleType::CornerTopLeft: {
            float anchor_right = drag.group_start_x + drag.group_start_w;
            float anchor_bottom = drag.group_start_y + drag.group_start_h;
            new_left = std::clamp(local_x, 0.0f, anchor_right - kMinSize);
            new_top = std::clamp(local_y, 0.0f, anchor_bottom - kMinSize);
            if (snap_enabled) {
                new_left = snap_single(new_left);
                new_top = snap_single(new_top);
            }
            snap_edge_value(new_left, drag.snap_edges_x);
            snap_edge_value(new_top, drag.snap_edges_y);
            new_width = std::max(kMinSize, anchor_right - new_left);
            new_height = std::max(kMinSize, anchor_bottom - new_top);
            apply_member_changes(new_left, new_top, new_width, new_height);
            return changed;
        }
        case HandleType::CornerTopRight: {
            float anchor_left = drag.group_start_x;
            float anchor_bottom = drag.group_start_y + drag.group_start_h;
            float new_right = std::clamp(local_x, anchor_left + kMinSize, 1.0f);
            new_top = std::clamp(local_y, 0.0f, anchor_bottom - kMinSize);
            if (snap_enabled) {
                new_right = snap_single(new_right);
                new_top = snap_single(new_top);
            }
            snap_edge_value(new_right, drag.snap_edges_x);
            snap_edge_value(new_top, drag.snap_edges_y);
            new_width = std::max(kMinSize, new_right - anchor_left);
            new_height = std::max(kMinSize, anchor_bottom - new_top);
            apply_member_changes(anchor_left, new_top, new_width, new_height);
            return changed;
        }
        case HandleType::CornerBottomLeft: {
            float anchor_right = drag.group_start_x + drag.group_start_w;
            float anchor_top = drag.group_start_y;
            new_left = std::clamp(local_x, 0.0f, anchor_right - kMinSize);
            float new_bottom = std::clamp(local_y, anchor_top + kMinSize, 1.0f);
            if (snap_enabled) {
                new_left = snap_single(new_left);
                new_bottom = snap_single(new_bottom);
            }
            snap_edge_value(new_left, drag.snap_edges_x);
            snap_edge_value(new_bottom, drag.snap_edges_y);
            new_width = std::max(kMinSize, anchor_right - new_left);
            new_height = std::max(kMinSize, new_bottom - anchor_top);
            apply_member_changes(new_left, anchor_top, new_width, new_height);
            return changed;
        }
        case HandleType::CornerBottomRight: {
            float anchor_left = drag.group_start_x;
            float anchor_top = drag.group_start_y;
            float new_right = std::clamp(local_x, anchor_left + kMinSize, 1.0f);
            float new_bottom = std::clamp(local_y, anchor_top + kMinSize, 1.0f);
            if (snap_enabled) {
                new_right = snap_single(new_right);
                new_bottom = snap_single(new_bottom);
            }
            snap_edge_value(new_right, drag.snap_edges_x);
            snap_edge_value(new_bottom, drag.snap_edges_y);
            new_width = std::max(kMinSize, new_right - anchor_left);
            new_height = std::max(kMinSize, new_bottom - anchor_top);
            apply_member_changes(anchor_left, anchor_top, new_width, new_height);
            return changed;
        }
        }
        return changed;
    }

    if (drag.object_index < 0 || drag.object_index >= static_cast<int>(layout.objects.size())) {
        layout_editor_end_drag(engine);
        return false;
    }

    auto& obj = layout.objects[static_cast<std::size_t>(drag.object_index)];
    bool changed = false;

    switch (drag.handle) {
    case HandleType::Center: {
        float new_x = local_x - drag.offset_x;
        float new_y = local_y - drag.offset_y;
        new_x = std::clamp(new_x, 0.0f, 1.0f - obj.rect.w);
        new_y = std::clamp(new_y, 0.0f, 1.0f - obj.rect.h);
        if (snap_enabled && grid_step > 0.0f) {
            new_x = apply_snap_pos(new_x, obj.rect.w);
            new_y = apply_snap_pos(new_y, obj.rect.h);
        }
        snap_axis(new_x, obj.rect.w, drag.snap_edges_x);
        snap_axis(new_y, obj.rect.h, drag.snap_edges_y);
        changed =
            (std::fabs(obj.rect.x - new_x) > 1e-5f) || (std::fabs(obj.rect.y - new_y) > 1e-5f);
        obj.rect.x = new_x;
        obj.rect.y = new_y;
        break;
    }
    case HandleType::EdgeLeft: {
        float anchor = drag.start_x + drag.start_w;
        float new_left = std::clamp(local_x, 0.0f, anchor - kMinSize);
        if (snap_enabled)
            new_left = snap_single(new_left);
        snap_edge_value(new_left, drag.snap_edges_x);
        float new_width = anchor - new_left;
        changed = (std::fabs(obj.rect.x - new_left) > 1e-5f) ||
                  (std::fabs(obj.rect.w - new_width) > 1e-5f);
        obj.rect.x = new_left;
        obj.rect.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.rect.x));
        break;
    }
    case HandleType::EdgeRight: {
        float anchor = drag.start_x;
        float new_right = std::clamp(local_x, anchor + kMinSize, 1.0f);
        if (snap_enabled)
            new_right = snap_single(new_right);
        snap_edge_value(new_right, drag.snap_edges_x);
        float new_width = new_right - anchor;
        changed = (std::fabs(obj.rect.w - new_width) > 1e-5f);
        obj.rect.x = anchor;
        obj.rect.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.rect.x));
        break;
    }
    case HandleType::EdgeTop: {
        float anchor = drag.start_y + drag.start_h;
        float new_top = std::clamp(local_y, 0.0f, anchor - kMinSize);
        if (snap_enabled)
            new_top = snap_single(new_top);
        snap_edge_value(new_top, drag.snap_edges_y);
        float new_height = anchor - new_top;
        changed = (std::fabs(obj.rect.y - new_top) > 1e-5f) ||
                  (std::fabs(obj.rect.h - new_height) > 1e-5f);
        obj.rect.y = new_top;
        obj.rect.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.rect.y));
        break;
    }
    case HandleType::EdgeBottom: {
        float anchor = drag.start_y;
        float new_bottom = std::clamp(local_y, anchor + kMinSize, 1.0f);
        if (snap_enabled)
            new_bottom = snap_single(new_bottom);
        snap_edge_value(new_bottom, drag.snap_edges_y);
        float new_height = new_bottom - anchor;
        changed = (std::fabs(obj.rect.h - new_height) > 1e-5f);
        obj.rect.y = anchor;
        obj.rect.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.rect.y));
        break;
    }
    case HandleType::CornerTopLeft: {
        float anchor_right = drag.start_x + drag.start_w;
        float anchor_bottom = drag.start_y + drag.start_h;
        float new_left = std::clamp(local_x, 0.0f, anchor_right - kMinSize);
        float new_top = std::clamp(local_y, 0.0f, anchor_bottom - kMinSize);
        if (snap_enabled) {
            new_left = snap_single(new_left);
            new_top = snap_single(new_top);
        }
        snap_edge_value(new_left, drag.snap_edges_x);
        snap_edge_value(new_top, drag.snap_edges_y);
        float new_width = anchor_right - new_left;
        float new_height = anchor_bottom - new_top;
        changed = (std::fabs(obj.rect.x - new_left) > 1e-5f) ||
                  (std::fabs(obj.rect.y - new_top) > 1e-5f) ||
                  (std::fabs(obj.rect.w - new_width) > 1e-5f) ||
                  (std::fabs(obj.rect.h - new_height) > 1e-5f);
        obj.rect.x = new_left;
        obj.rect.y = new_top;
        obj.rect.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.rect.x));
        obj.rect.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.rect.y));
        break;
    }
    case HandleType::CornerTopRight: {
        float anchor_left = drag.start_x;
        float anchor_bottom = drag.start_y + drag.start_h;
        float new_right = std::clamp(local_x, anchor_left + kMinSize, 1.0f);
        float new_top = std::clamp(local_y, 0.0f, anchor_bottom - kMinSize);
        if (snap_enabled) {
            new_right = snap_single(new_right);
            new_top = snap_single(new_top);
        }
        snap_edge_value(new_right, drag.snap_edges_x);
        snap_edge_value(new_top, drag.snap_edges_y);
        float new_width = new_right - anchor_left;
        float new_height = anchor_bottom - new_top;
        changed = (std::fabs(obj.rect.w - new_width) > 1e-5f) ||
                  (std::fabs(obj.rect.y - new_top) > 1e-5f) ||
                  (std::fabs(obj.rect.h - new_height) > 1e-5f);
        obj.rect.x = anchor_left;
        obj.rect.y = new_top;
        obj.rect.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.rect.x));
        obj.rect.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.rect.y));
        break;
    }
    case HandleType::CornerBottomLeft: {
        float anchor_right = drag.start_x + drag.start_w;
        float anchor_top = drag.start_y;
        float new_left = std::clamp(local_x, 0.0f, anchor_right - kMinSize);
        float new_bottom = std::clamp(local_y, anchor_top + kMinSize, 1.0f);
        if (snap_enabled) {
            new_left = snap_single(new_left);
            new_bottom = snap_single(new_bottom);
        }
        snap_edge_value(new_left, drag.snap_edges_x);
        snap_edge_value(new_bottom, drag.snap_edges_y);
        float new_width = anchor_right - new_left;
        float new_height = new_bottom - anchor_top;
        changed = (std::fabs(obj.rect.x - new_left) > 1e-5f) ||
                  (std::fabs(obj.rect.h - new_height) > 1e-5f) ||
                  (std::fabs(obj.rect.w - new_width) > 1e-5f);
        obj.rect.x = new_left;
        obj.rect.y = anchor_top;
        obj.rect.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.rect.x));
        obj.rect.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.rect.y));
        break;
    }
    case HandleType::CornerBottomRight: {
        float anchor_left = drag.start_x;
        float anchor_top = drag.start_y;
        float new_right = std::clamp(local_x, anchor_left + kMinSize, 1.0f);
        float new_bottom = std::clamp(local_y, anchor_top + kMinSize, 1.0f);
        if (snap_enabled) {
            new_right = snap_single(new_right);
            new_bottom = snap_single(new_bottom);
        }
        snap_edge_value(new_right, drag.snap_edges_x);
        snap_edge_value(new_bottom, drag.snap_edges_y);
        float new_width = new_right - anchor_left;
        float new_height = new_bottom - anchor_top;
        changed = (std::fabs(obj.rect.w - new_width) > 1e-5f) ||
                  (std::fabs(obj.rect.h - new_height) > 1e-5f);
        obj.rect.x = anchor_left;
        obj.rect.y = anchor_top;
        obj.rect.w = std::max(kMinSize, std::min(new_width, 1.0f - obj.rect.x));
        obj.rect.h = std::max(kMinSize, std::min(new_height, 1.0f - obj.rect.y));
        break;
    }
    }
    return changed;
}

void layout_editor_end_drag(EngineState& engine) {
    editor_state(engine).drag = DragState{};
}

bool layout_editor_is_dragging(const EngineState& engine) {
    return editor_state(engine).drag.active;
}

int layout_editor_dragging_index(const EngineState& engine) {
    const DragState& drag = editor_state(engine).drag;
    if (!drag.active || drag.group)
        return -1;
    return drag.object_index;
}

HandleType layout_editor_drag_handle(const EngineState& engine) {
    return editor_state(engine).drag.handle;
}
