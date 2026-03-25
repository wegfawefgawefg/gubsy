#pragma once

#include <SDL2/SDL.h>

#include <vector>

struct UILayout;

struct LayoutEditorViewport {
    float origin_x{0.0f};
    float origin_y{0.0f};
    float width{0.0f};
    float height{0.0f};
};

enum class HitTarget {
    None,
    Object,
    Group,
};

enum class HandleType {
    Center,
    EdgeLeft,
    EdgeRight,
    EdgeTop,
    EdgeBottom,
    CornerTopLeft,
    CornerTopRight,
    CornerBottomLeft,
    CornerBottomRight,
};

struct HitResult {
    HitTarget target{HitTarget::None};
    int object_index{-1};
    HandleType handle{HandleType::Center};
};

inline constexpr float kHandleSize = 12.0f;
inline constexpr float kEdgeHandleLength = 18.0f;
inline constexpr float kEdgeHandleThickness = 6.0f;

void layout_editor_set_viewport(const LayoutEditorViewport& viewport);
LayoutEditorViewport layout_editor_get_viewport();

bool layout_editor_hit_test(const UILayout& layout,
                            const LayoutEditorViewport& viewport,
                            float mouse_x,
                            float mouse_y,
                            HitResult& out_hit);

void layout_editor_begin_drag(const UILayout& layout,
                              const HitResult& hit,
                              float mouse_x,
                              float mouse_y,
                              const LayoutEditorViewport& viewport);

bool layout_editor_update_drag(UILayout& layout,
                               float mouse_x,
                               float mouse_y,
                               bool snap_enabled,
                               float grid_step);

void layout_editor_end_drag();
bool layout_editor_is_dragging();
int layout_editor_dragging_index();
HandleType layout_editor_drag_handle();

// Selection management
const std::vector<int>& layout_editor_selection_indices();
int layout_editor_selection_count();
bool layout_editor_is_selected(int index);
int layout_editor_primary_selection();
void layout_editor_select_single(int index);
void layout_editor_add_to_selection(int index);
void layout_editor_remove_from_selection(int index);
void layout_editor_clear_selection();
bool layout_editor_selection_bounds(const UILayout& layout,
                                    float& min_x,
                                    float& min_y,
                                    float& max_x,
                                    float& max_y);
