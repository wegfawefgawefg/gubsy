#pragma once

#include <SDL2/SDL.h>

struct UILayout;

struct LayoutEditorViewport {
    float origin_x{0.0f};
    float origin_y{0.0f};
    float width{0.0f};
    float height{0.0f};
};

void layout_editor_set_viewport(const LayoutEditorViewport& viewport);
LayoutEditorViewport layout_editor_get_viewport();

int layout_editor_selected_index();
void layout_editor_select(int index);
void layout_editor_clear_selection();

bool layout_editor_hit_test(const UILayout& layout,
                            const LayoutEditorViewport& viewport,
                            float mouse_x,
                            float mouse_y,
                            int& out_index);

void layout_editor_begin_drag(const UILayout& layout,
                              int object_index,
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
