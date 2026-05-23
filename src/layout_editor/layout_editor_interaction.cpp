#include "src/layout_editor/layout_editor_interaction.hpp"

#include "src/engine_state.hpp"
#include "src/layout_editor/layout_editor_internal.hpp"

namespace {

HandleType from_glayout_handle(glayout::Handle handle) {
    switch (handle) {
    case glayout::Handle::Center:
        return HandleType::Center;
    case glayout::Handle::Left:
        return HandleType::EdgeLeft;
    case glayout::Handle::Right:
        return HandleType::EdgeRight;
    case glayout::Handle::Top:
        return HandleType::EdgeTop;
    case glayout::Handle::Bottom:
        return HandleType::EdgeBottom;
    case glayout::Handle::TopLeft:
        return HandleType::CornerTopLeft;
    case glayout::Handle::TopRight:
        return HandleType::CornerTopRight;
    case glayout::Handle::BottomLeft:
        return HandleType::CornerBottomLeft;
    case glayout::Handle::BottomRight:
        return HandleType::CornerBottomRight;
    case glayout::Handle::None:
        break;
    }
    return HandleType::Center;
}

} // namespace

void layout_editor_set_viewport(EngineState& engine, const LayoutEditorViewport& viewport) {
    layout_editor_internal::editor_state(engine).viewport = viewport;
}

LayoutEditorViewport layout_editor_get_viewport(const EngineState& engine) {
    return layout_editor_internal::editor_state(engine).viewport;
}

bool layout_editor_is_dragging(const EngineState& engine) {
    return layout_editor_internal::editor_state(engine).editor.dragging;
}

int layout_editor_dragging_index(const EngineState& engine) {
    const auto& editor = layout_editor_internal::editor_state(engine).editor;
    if (!editor.dragging || editor.primary < 0)
        return -1;
    return editor.primary;
}

HandleType layout_editor_drag_handle(const EngineState& engine) {
    return from_glayout_handle(layout_editor_internal::editor_state(engine).editor.drag_handle);
}

const std::vector<int>& layout_editor_selection_indices(const EngineState& engine) {
    return layout_editor_internal::editor_state(engine).editor.selection;
}

int layout_editor_selection_count(const EngineState& engine) {
    return static_cast<int>(layout_editor_internal::editor_state(engine).editor.selection.size());
}

bool layout_editor_is_selected(const EngineState& engine, int index) {
    return glayout::editor_is_selected(layout_editor_internal::editor_state(engine).editor, index);
}

int layout_editor_primary_selection(const EngineState& engine) {
    return layout_editor_internal::editor_state(engine).editor.primary;
}

void layout_editor_select_single(EngineState& engine, int index) {
    glayout::editor_select_single(layout_editor_internal::editor_state(engine).editor, index);
}

void layout_editor_add_to_selection(EngineState& engine, int index) {
    glayout::editor_add_to_selection(layout_editor_internal::editor_state(engine).editor, index);
}

void layout_editor_remove_from_selection(EngineState& engine, int index) {
    glayout::editor_remove_from_selection(layout_editor_internal::editor_state(engine).editor,
                                          index);
}

void layout_editor_clear_selection(EngineState& engine) {
    glayout::editor_clear_selection(layout_editor_internal::editor_state(engine).editor);
}

bool layout_editor_selection_bounds(const EngineState& engine, const UILayout& layout, float& min_x,
                                    float& min_y, float& max_x, float& max_y) {
    glayout::Rect bounds{};
    if (!glayout::editor_selection_bounds(layout_editor_internal::editor_state(engine).editor,
                                          layout, bounds)) {
        return false;
    }

    min_x = bounds.x;
    min_y = bounds.y;
    max_x = bounds.x + bounds.w;
    max_y = bounds.y + bounds.h;
    return true;
}
