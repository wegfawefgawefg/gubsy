#include "src/layout_editor/layout_editor_history.hpp"

#include "src/engine_state.hpp"
#include "src/layout_editor/layout_editor_internal.hpp"

void layout_editor_history_reset(EngineState& engine, const UILayout& layout) {
    auto& editor = layout_editor_internal::editor_state(engine).editor;
    editor.undo_stack.clear();
    editor.redo_stack.clear();
    glayout::editor_commit_undo(editor, layout);
}

void layout_editor_history_commit(EngineState& engine, const UILayout& layout) {
    glayout::editor_commit_undo(layout_editor_internal::editor_state(engine).editor, layout);
}

bool layout_editor_history_undo(EngineState& engine, UILayout& layout) {
    return glayout::editor_undo(layout_editor_internal::editor_state(engine).editor, layout);
}

bool layout_editor_history_redo(EngineState& engine, UILayout& layout) {
    return glayout::editor_redo(layout_editor_internal::editor_state(engine).editor, layout);
}

void layout_editor_history_shutdown(EngineState& engine) {
    auto& editor = layout_editor_internal::editor_state(engine).editor;
    editor.undo_stack.clear();
    editor.redo_stack.clear();
}
