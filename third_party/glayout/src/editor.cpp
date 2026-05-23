#include "glayout/editor.hpp"

#include <algorithm>
#include <cmath>

namespace glayout {
namespace {

constexpr float kMinSize = 0.01f;
constexpr float kHandlePixels = 8.0f;
constexpr float kPasteNudge = 0.02f;
constexpr float kSnapEpsilon = 0.01f;
constexpr std::size_t kMaxHistoryEntries = 64;

bool contains(Rect rect, float x, float y) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

float snap(float value, float step) {
    if (step <= 0.0f)
        return value;
    return std::round(value / step) * step;
}

float maybe_snap(const EditorState& editor, float value) {
    if (!editor.snap_enabled)
        return value;
    return snap(value, editor.grid_step);
}

float clamp_unit_position(float value, float size) {
    float max_position = std::max(0.0f, 1.0f - size);
    return std::clamp(value, 0.0f, max_position);
}

Rect object_to_screen(const Object& object, Viewport viewport) {
    return map_rect(Rect{viewport.x, viewport.y, viewport.w, viewport.h}, object.rect);
}

Rect handle_rect(float cx, float cy, float size) {
    float half = size * 0.5f;
    return Rect{cx - half, cy - half, size, size};
}

Handle hit_handle(Rect rect, float mouse_x, float mouse_y) {
    if (contains(handle_rect(rect.x, rect.y, kHandlePixels), mouse_x, mouse_y))
        return Handle::TopLeft;
    if (contains(handle_rect(rect.x + rect.w, rect.y, kHandlePixels), mouse_x, mouse_y))
        return Handle::TopRight;
    if (contains(handle_rect(rect.x, rect.y + rect.h, kHandlePixels), mouse_x, mouse_y))
        return Handle::BottomLeft;
    if (contains(handle_rect(rect.x + rect.w, rect.y + rect.h, kHandlePixels), mouse_x, mouse_y))
        return Handle::BottomRight;
    if (contains(Rect{rect.x - kHandlePixels * 0.5f, rect.y, kHandlePixels, rect.h}, mouse_x,
                 mouse_y)) {
        return Handle::Left;
    }
    if (contains(Rect{rect.x + rect.w - kHandlePixels * 0.5f, rect.y, kHandlePixels, rect.h},
                 mouse_x, mouse_y)) {
        return Handle::Right;
    }
    if (contains(Rect{rect.x, rect.y - kHandlePixels * 0.5f, rect.w, kHandlePixels}, mouse_x,
                 mouse_y)) {
        return Handle::Top;
    }
    if (contains(Rect{rect.x, rect.y + rect.h - kHandlePixels * 0.5f, rect.w, kHandlePixels},
                 mouse_x, mouse_y)) {
        return Handle::Bottom;
    }
    if (contains(rect, mouse_x, mouse_y))
        return Handle::Center;
    return Handle::None;
}

void sync_primary(EditorState& editor) {
    if (editor.selection.empty()) {
        editor.primary = -1;
        return;
    }
    if (!editor_is_selected(editor, editor.primary))
        editor.primary = editor.selection.back();
}

bool valid_object_index(const Layout& layout, int index) {
    return index >= 0 && index < static_cast<int>(layout.objects.size());
}

void prune_selection(EditorState& editor, const Layout& layout) {
    auto it = std::remove_if(editor.selection.begin(), editor.selection.end(),
                             [&](int index) { return !valid_object_index(layout, index); });
    editor.selection.erase(it, editor.selection.end());
    sync_primary(editor);
}

bool selection_bounds_from_indices(const Layout& layout, const std::vector<int>& selection,
                                   Rect& out_bounds) {
    bool any = false;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    for (int index : selection) {
        if (!valid_object_index(layout, index))
            continue;
        const Rect& rect = layout.objects[static_cast<std::size_t>(index)].rect;
        if (!any) {
            min_x = rect.x;
            min_y = rect.y;
            max_x = rect.x + rect.w;
            max_y = rect.y + rect.h;
            any = true;
        } else {
            min_x = std::min(min_x, rect.x);
            min_y = std::min(min_y, rect.y);
            max_x = std::max(max_x, rect.x + rect.w);
            max_y = std::max(max_y, rect.y + rect.h);
        }
    }
    if (!any)
        return false;
    out_bounds = Rect{min_x, min_y, max_x - min_x, max_y - min_y};
    return true;
}

float snap_to_neighbors(float value, const std::vector<float>& edges) {
    for (float edge : edges) {
        if (std::fabs(value - edge) <= kSnapEpsilon)
            return edge;
    }
    return value;
}

void collect_neighbor_edges(const Layout& layout, const std::vector<int>& selection,
                            std::vector<float>& x_edges, std::vector<float>& y_edges) {
    x_edges.clear();
    y_edges.clear();
    x_edges.push_back(0.0f);
    x_edges.push_back(0.5f);
    x_edges.push_back(1.0f);
    y_edges.push_back(0.0f);
    y_edges.push_back(0.5f);
    y_edges.push_back(1.0f);

    for (int index = 0; index < static_cast<int>(layout.objects.size()); ++index) {
        if (std::find(selection.begin(), selection.end(), index) != selection.end())
            continue;
        const Rect& rect = layout.objects[static_cast<std::size_t>(index)].rect;
        x_edges.push_back(rect.x);
        x_edges.push_back(rect.x + rect.w * 0.5f);
        x_edges.push_back(rect.x + rect.w);
        y_edges.push_back(rect.y);
        y_edges.push_back(rect.y + rect.h * 0.5f);
        y_edges.push_back(rect.y + rect.h);
    }
}

void begin_drag(EditorState& editor, const Layout& layout, const HitResult& hit, float local_x,
                float local_y) {
    editor.dragging = hit.handle != Handle::None;
    editor.drag_changed = false;
    editor.drag_handle = hit.handle;
    editor.drag_start_x = local_x;
    editor.drag_start_y = local_y;
    editor.drag_group_bounds = {};
    editor.drag_start_selection = editor.selection;
    editor.drag_start_objects.clear();
    editor.drag_start_objects.reserve(editor.selection.size());
    (void)selection_bounds_from_indices(layout, editor.drag_start_selection,
                                        editor.drag_group_bounds);

    for (int index : editor.selection) {
        if (valid_object_index(layout, index))
            editor.drag_start_objects.push_back(layout.objects[static_cast<std::size_t>(index)]);
    }
}

void translate_selection(EditorState& editor, Layout& layout, float dx, float dy) {
    std::vector<float> x_edges;
    std::vector<float> y_edges;
    collect_neighbor_edges(layout, editor.drag_start_selection, x_edges, y_edges);
    if (editor.snap_enabled) {
        float snapped_left = snap_to_neighbors(editor.drag_group_bounds.x + dx, x_edges);
        float snapped_right = snap_to_neighbors(
            editor.drag_group_bounds.x + editor.drag_group_bounds.w + dx, x_edges);
        float snapped_top = snap_to_neighbors(editor.drag_group_bounds.y + dy, y_edges);
        float snapped_bottom = snap_to_neighbors(
            editor.drag_group_bounds.y + editor.drag_group_bounds.h + dy, y_edges);
        if (std::fabs(snapped_left - (editor.drag_group_bounds.x + dx)) <=
            std::fabs(snapped_right -
                      (editor.drag_group_bounds.x + editor.drag_group_bounds.w + dx)))
            dx = snapped_left - editor.drag_group_bounds.x;
        else
            dx = snapped_right - (editor.drag_group_bounds.x + editor.drag_group_bounds.w);
        if (std::fabs(snapped_top - (editor.drag_group_bounds.y + dy)) <=
            std::fabs(snapped_bottom -
                      (editor.drag_group_bounds.y + editor.drag_group_bounds.h + dy)))
            dy = snapped_top - editor.drag_group_bounds.y;
        else
            dy = snapped_bottom - (editor.drag_group_bounds.y + editor.drag_group_bounds.h);
    }

    for (std::size_t i = 0; i < editor.drag_start_selection.size(); ++i) {
        int index = editor.drag_start_selection[i];
        if (!valid_object_index(layout, index) || i >= editor.drag_start_objects.size())
            continue;

        const Object& start = editor.drag_start_objects[i];
        Object& object = layout.objects[static_cast<std::size_t>(index)];
        object.rect.x = clamp_unit_position(maybe_snap(editor, start.rect.x + dx), object.rect.w);
        object.rect.y = clamp_unit_position(maybe_snap(editor, start.rect.y + dy), object.rect.h);
    }
}

bool nudge_selection(EditorState& editor, Layout& layout, float dx, float dy) {
    if (editor.selection.empty())
        return false;

    bool changed = false;
    for (int index : editor.selection) {
        if (!valid_object_index(layout, index))
            continue;

        Object& object = layout.objects[static_cast<std::size_t>(index)];
        float next_x = clamp_unit_position(object.rect.x + dx, object.rect.w);
        float next_y = clamp_unit_position(object.rect.y + dy, object.rect.h);
        if (next_x == object.rect.x && next_y == object.rect.y)
            continue;

        object.rect.x = next_x;
        object.rect.y = next_y;
        changed = true;
    }

    return changed;
}

void resize_group(EditorState& editor, Layout& layout, float dx, float dy) {
    if (editor.drag_start_selection.empty() || editor.drag_start_objects.empty())
        return;

    Rect bounds = editor.drag_group_bounds;
    Rect next = bounds;
    switch (editor.drag_handle) {
    case Handle::Left:
    case Handle::TopLeft:
    case Handle::BottomLeft:
        next.x = bounds.x + dx;
        next.w = bounds.w - dx;
        break;
    default:
        break;
    }
    switch (editor.drag_handle) {
    case Handle::Right:
    case Handle::TopRight:
    case Handle::BottomRight:
        next.w = bounds.w + dx;
        break;
    default:
        break;
    }
    switch (editor.drag_handle) {
    case Handle::Top:
    case Handle::TopLeft:
    case Handle::TopRight:
        next.y = bounds.y + dy;
        next.h = bounds.h - dy;
        break;
    default:
        break;
    }
    switch (editor.drag_handle) {
    case Handle::Bottom:
    case Handle::BottomLeft:
    case Handle::BottomRight:
        next.h = bounds.h + dy;
        break;
    default:
        break;
    }

    next.x = maybe_snap(editor, next.x);
    next.y = maybe_snap(editor, next.y);
    next.w = maybe_snap(editor, next.w);
    next.h = maybe_snap(editor, next.h);
    next.w = std::clamp(next.w, kMinSize, 1.0f);
    next.h = std::clamp(next.h, kMinSize, 1.0f);
    next.x = clamp_unit_position(next.x, next.w);
    next.y = clamp_unit_position(next.y, next.h);

    float sx = bounds.w > 0.0f ? next.w / bounds.w : 1.0f;
    float sy = bounds.h > 0.0f ? next.h / bounds.h : 1.0f;
    for (std::size_t i = 0; i < editor.drag_start_selection.size(); ++i) {
        int index = editor.drag_start_selection[i];
        if (!valid_object_index(layout, index) || i >= editor.drag_start_objects.size())
            continue;

        const Rect& start = editor.drag_start_objects[i].rect;
        Rect rect;
        rect.x = next.x + (start.x - bounds.x) * sx;
        rect.y = next.y + (start.y - bounds.y) * sy;
        rect.w = std::max(kMinSize, start.w * sx);
        rect.h = std::max(kMinSize, start.h * sy);
        rect.x = clamp_unit_position(rect.x, rect.w);
        rect.y = clamp_unit_position(rect.y, rect.h);
        layout.objects[static_cast<std::size_t>(index)].rect = rect;
    }
}

void resize_primary(EditorState& editor, Layout& layout, float dx, float dy) {
    if (editor.drag_start_objects.empty() || editor.primary < 0)
        return;
    if (!valid_object_index(layout, editor.primary))
        return;

    const Object* start_object = nullptr;
    for (std::size_t i = 0; i < editor.drag_start_selection.size(); ++i) {
        if (editor.drag_start_selection[i] == editor.primary &&
            i < editor.drag_start_objects.size()) {
            start_object = &editor.drag_start_objects[i];
            break;
        }
    }
    if (!start_object)
        return;

    const Object& start = *start_object;
    Rect rect = start.rect;

    switch (editor.drag_handle) {
    case Handle::Left:
    case Handle::TopLeft:
    case Handle::BottomLeft:
        rect.x = start.rect.x + dx;
        rect.w = start.rect.w - dx;
        break;
    default:
        break;
    }

    switch (editor.drag_handle) {
    case Handle::Right:
    case Handle::TopRight:
    case Handle::BottomRight:
        rect.w = start.rect.w + dx;
        break;
    default:
        break;
    }

    switch (editor.drag_handle) {
    case Handle::Top:
    case Handle::TopLeft:
    case Handle::TopRight:
        rect.y = start.rect.y + dy;
        rect.h = start.rect.h - dy;
        break;
    default:
        break;
    }

    switch (editor.drag_handle) {
    case Handle::Bottom:
    case Handle::BottomLeft:
    case Handle::BottomRight:
        rect.h = start.rect.h + dy;
        break;
    default:
        break;
    }

    rect.x = maybe_snap(editor, rect.x);
    rect.y = maybe_snap(editor, rect.y);
    rect.w = maybe_snap(editor, rect.w);
    rect.h = maybe_snap(editor, rect.h);

    rect.w = std::clamp(rect.w, kMinSize, 1.0f);
    rect.h = std::clamp(rect.h, kMinSize, 1.0f);
    rect.x = clamp_unit_position(rect.x, rect.w);
    rect.y = clamp_unit_position(rect.y, rect.h);

    layout.objects[static_cast<std::size_t>(editor.primary)].rect = rect;
}

bool layout_changed(const Layout& a, const Layout& b) {
    if (a.id != b.id || a.label != b.label || a.width != b.width || a.height != b.height ||
        a.form_factor != b.form_factor || a.objects.size() != b.objects.size()) {
        return true;
    }

    for (std::size_t i = 0; i < a.objects.size(); ++i) {
        const Object& left = a.objects[i];
        const Object& right = b.objects[i];
        if (left.id != right.id || left.label != right.label)
            return true;
        if (left.rect.x != right.rect.x || left.rect.y != right.rect.y ||
            left.rect.w != right.rect.w || left.rect.h != right.rect.h) {
            return true;
        }
    }

    return false;
}

int next_object_id(const EditorInput& input, const Layout& layout) {
    if (input.generate_object_id)
        return input.generate_object_id(layout, input.generate_object_id_user_data);
    return generate_object_id(layout);
}

void commit_snapshot(EditorState& editor, const Layout& layout) {
    if (!editor.undo_stack.empty() && !layout_changed(editor.undo_stack.back(), layout))
        return;

    editor.undo_stack.push_back(layout);
    editor.redo_stack.clear();
    if (editor.undo_stack.size() > kMaxHistoryEntries)
        editor.undo_stack.erase(editor.undo_stack.begin());
}

void ensure_history_started(EditorState& editor, const Layout& layout) {
    if (editor.undo_stack.empty())
        commit_snapshot(editor, layout);
}

} // namespace

bool editor_hit_test(const Layout& layout, Viewport viewport, float mouse_x, float mouse_y,
                     const std::vector<int>& selection, HitResult& out_hit) {
    out_hit = HitResult{};
    if (viewport.w <= 0.0f || viewport.h <= 0.0f)
        return false;

    if (selection.size() > 1) {
        Rect bounds;
        if (selection_bounds_from_indices(layout, selection, bounds)) {
            Rect screen_bounds =
                map_rect(Rect{viewport.x, viewport.y, viewport.w, viewport.h}, bounds);
            Handle handle = hit_handle(screen_bounds, mouse_x, mouse_y);
            if (handle != Handle::None) {
                out_hit.object_index = -1;
                out_hit.handle = handle;
                return true;
            }
        }
    }

    for (int index : selection) {
        if (!valid_object_index(layout, index))
            continue;
        Rect rect = object_to_screen(layout.objects[static_cast<std::size_t>(index)], viewport);
        Handle handle = hit_handle(rect, mouse_x, mouse_y);
        if (handle != Handle::None) {
            out_hit.object_index = index;
            out_hit.handle = handle;
            return true;
        }
    }

    for (int index = static_cast<int>(layout.objects.size()) - 1; index >= 0; --index) {
        Rect rect = object_to_screen(layout.objects[static_cast<std::size_t>(index)], viewport);
        Handle handle = hit_handle(rect, mouse_x, mouse_y);
        if (handle != Handle::None) {
            out_hit.object_index = index;
            out_hit.handle = handle;
            return true;
        }
    }

    return false;
}

EditorFrameResult editor_begin_frame(EditorState& editor, Layout& layout, const EditorInput& input,
                                     Viewport viewport) {
    EditorFrameResult result;
    prune_selection(editor, layout);

    if (input.key_save) {
        editor.save_requested = true;
        result.save_requested = true;
    }
    if (input.key_undo && editor_undo(editor, layout)) {
        result.changed = true;
        editor.dirty = true;
    }
    if (input.key_redo && editor_redo(editor, layout)) {
        result.changed = true;
        editor.dirty = true;
    }
    if (input.key_delete && !editor.selection.empty()) {
        ensure_history_started(editor, layout);
        std::vector<int> to_remove = editor.selection;
        std::sort(to_remove.begin(), to_remove.end());
        for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
            if (valid_object_index(layout, *it))
                layout.objects.erase(layout.objects.begin() + *it);
        }
        editor_clear_selection(editor);
        editor_commit_undo(editor, layout);
        editor.dirty = true;
        result.changed = true;
        result.selection_changed = true;
    }
    if (input.key_copy && !editor.selection.empty()) {
        editor.clipboard.clear();
        for (int index : editor.selection) {
            if (valid_object_index(layout, index))
                editor.clipboard.push_back(layout.objects[static_cast<std::size_t>(index)]);
        }
    }
    if (input.key_paste && !editor.clipboard.empty()) {
        ensure_history_started(editor, layout);
        editor_clear_selection(editor);
        for (const Object& object : editor.clipboard) {
            Object copy = object;
            copy.id = next_object_id(input, layout);
            copy.rect.x = clamp_unit_position(copy.rect.x + kPasteNudge, copy.rect.w);
            copy.rect.y = clamp_unit_position(copy.rect.y + kPasteNudge, copy.rect.h);
            layout.objects.push_back(copy);
            editor_add_to_selection(editor, static_cast<int>(layout.objects.size()) - 1);
        }
        editor_commit_undo(editor, layout);
        editor.dirty = true;
        result.changed = true;
        result.selection_changed = true;
    }
    if ((input.nudge_x != 0.0f || input.nudge_y != 0.0f) && !editor.selection.empty()) {
        ensure_history_started(editor, layout);
        if (nudge_selection(editor, layout, input.nudge_x, input.nudge_y)) {
            editor_commit_undo(editor, layout);
            editor.dirty = true;
            result.changed = true;
        }
    }

    float local_x = viewport.w > 0.0f ? (input.mouse_x - viewport.x) / viewport.w : 0.0f;
    float local_y = viewport.h > 0.0f ? (input.mouse_y - viewport.y) / viewport.h : 0.0f;

    if (input.left_down && !editor.mouse_was_down) {
        HitResult hit;
        if (editor_hit_test(layout, viewport, input.mouse_x, input.mouse_y, editor.selection,
                            hit)) {
            if (hit.object_index < 0) {
                begin_drag(editor, layout, hit, local_x, local_y);
            } else if (input.ctrl || input.shift) {
                if (editor_is_selected(editor, hit.object_index))
                    editor_remove_from_selection(editor, hit.object_index);
                else
                    editor_add_to_selection(editor, hit.object_index);
                result.selection_changed = true;
            } else if (!editor_is_selected(editor, hit.object_index)) {
                editor_select_single(editor, hit.object_index);
                result.selection_changed = true;
            }

            if (hit.object_index >= 0 && editor_is_selected(editor, hit.object_index)) {
                editor.primary = hit.object_index;
                begin_drag(editor, layout, hit, local_x, local_y);
            }
        } else if (!input.ctrl && !input.shift) {
            editor_clear_selection(editor);
            result.selection_changed = true;
        }
    }

    if (input.left_down && editor.dragging) {
        float dx = local_x - editor.drag_start_x;
        float dy = local_y - editor.drag_start_y;
        Layout before = layout;

        if (editor.drag_handle == Handle::Center) {
            translate_selection(editor, layout, dx, dy);
        } else if (editor.drag_start_selection.size() > 1) {
            resize_group(editor, layout, dx, dy);
        } else {
            resize_primary(editor, layout, dx, dy);
        }

        if (layout_changed(before, layout)) {
            if (!editor.drag_changed)
                editor_commit_undo(editor, before);
            editor.drag_changed = true;
            editor.dirty = true;
            result.changed = true;
        }
    }

    if (!input.left_down && editor.mouse_was_down && editor.dragging) {
        if (editor.drag_changed)
            editor_commit_undo(editor, layout);
        editor.dragging = false;
        editor.drag_changed = false;
        editor.drag_start_objects.clear();
        editor.drag_start_selection.clear();
    }

    editor.mouse_was_down = input.left_down;
    return result;
}

void editor_clear_selection(EditorState& editor) {
    editor.selection.clear();
    editor.primary = -1;
}

void editor_select_single(EditorState& editor, int object_index) {
    editor.selection.clear();
    if (object_index >= 0)
        editor.selection.push_back(object_index);
    editor.primary = object_index;
}

void editor_add_to_selection(EditorState& editor, int object_index) {
    if (object_index < 0)
        return;
    if (!editor_is_selected(editor, object_index)) {
        editor.selection.push_back(object_index);
        std::sort(editor.selection.begin(), editor.selection.end());
    }
    editor.primary = object_index;
}

void editor_remove_from_selection(EditorState& editor, int object_index) {
    auto it = std::remove(editor.selection.begin(), editor.selection.end(), object_index);
    editor.selection.erase(it, editor.selection.end());
    sync_primary(editor);
}

bool editor_is_selected(const EditorState& editor, int object_index) {
    return std::find(editor.selection.begin(), editor.selection.end(), object_index) !=
           editor.selection.end();
}

bool editor_selection_bounds(const EditorState& editor, const Layout& layout, Rect& out_bounds) {
    return selection_bounds_from_indices(layout, editor.selection, out_bounds);
}

bool editor_nudge_selection(EditorState& editor, Layout& layout, float dx, float dy) {
    if (dx == 0.0f && dy == 0.0f)
        return false;

    ensure_history_started(editor, layout);
    if (nudge_selection(editor, layout, dx, dy)) {
        editor_commit_undo(editor, layout);
        editor.dirty = true;
        return true;
    }

    return false;
}

void editor_mark_saved(EditorState& editor) {
    editor.dirty = false;
    editor.save_requested = false;
}

void editor_commit_undo(EditorState& editor, const Layout& layout) {
    commit_snapshot(editor, layout);
}

bool editor_undo(EditorState& editor, Layout& layout) {
    if (editor.undo_stack.size() <= 1)
        return false;

    editor.redo_stack.push_back(editor.undo_stack.back());
    editor.undo_stack.pop_back();
    layout = editor.undo_stack.back();
    prune_selection(editor, layout);
    return true;
}

bool editor_redo(EditorState& editor, Layout& layout) {
    if (editor.redo_stack.empty())
        return false;

    layout = editor.redo_stack.back();
    editor.undo_stack.push_back(layout);
    editor.redo_stack.pop_back();
    prune_selection(editor, layout);
    return true;
}

} // namespace glayout
