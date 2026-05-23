#pragma once

#include "src/ui_layouts.hpp"

#include <SDL2/SDL.h>
#include <vector>

struct EngineState;

struct LayoutEditorViewport {
    float origin_x{0.0f};
    float origin_y{0.0f};
    float width{0.0f};
    float height{0.0f};
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

inline constexpr float kHandleSize = 12.0f;
inline constexpr float kEdgeHandleLength = 18.0f;
inline constexpr float kEdgeHandleThickness = 6.0f;

void layout_editor_set_viewport(EngineState& engine, const LayoutEditorViewport& viewport);
LayoutEditorViewport layout_editor_get_viewport(const EngineState& engine);

bool layout_editor_is_dragging(const EngineState& engine);
int layout_editor_dragging_index(const EngineState& engine);
HandleType layout_editor_drag_handle(const EngineState& engine);

// Selection management
const std::vector<int>& layout_editor_selection_indices(const EngineState& engine);
int layout_editor_selection_count(const EngineState& engine);
bool layout_editor_is_selected(const EngineState& engine, int index);
int layout_editor_primary_selection(const EngineState& engine);
void layout_editor_select_single(EngineState& engine, int index);
void layout_editor_add_to_selection(EngineState& engine, int index);
void layout_editor_remove_from_selection(EngineState& engine, int index);
void layout_editor_clear_selection(EngineState& engine);
bool layout_editor_selection_bounds(const EngineState& engine, const UILayout& layout, float& min_x,
                                    float& min_y, float& max_x, float& max_y);
