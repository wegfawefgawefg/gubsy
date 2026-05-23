#include "engine/layout_editor/layout_editor_history.hpp"

#include "engine/engine_state.hpp"
#include "engine/layout_editor/layout_editor_internal.hpp"
#include "engine/ui_layouts.hpp"

namespace {

using layout_editor_internal::LayoutEditorState;
using layout_editor_internal::LayoutSnapshot;

constexpr std::size_t kMaxHistoryEntries = 64;

LayoutSnapshot capture_snapshot(const UILayout& layout) {
    LayoutSnapshot snap;
    snap.layout_id = layout.id;
    snap.width = layout.width;
    snap.height = layout.height;
    snap.objects = layout.objects;
    return snap;
}

bool matches_tracked_layout(const LayoutEditorState& state, const UILayout& layout) {
    if (state.history.empty())
        return false;
    const LayoutSnapshot& snap = state.history.front();
    return snap.layout_id == layout.id && snap.width == layout.width &&
           snap.height == layout.height;
}

void apply_snapshot(UILayout& layout, const LayoutSnapshot& snapshot) {
    layout.objects = snapshot.objects;
}

} // namespace

void layout_editor_history_reset(EngineState& engine, const UILayout& layout) {
    LayoutEditorState& state = layout_editor_internal::editor_state(engine);
    state.history.clear();
    state.history.push_back(capture_snapshot(layout));
    state.history_index = 0;
}

void layout_editor_history_commit(EngineState& engine, const UILayout& layout) {
    LayoutEditorState& state = layout_editor_internal::editor_state(engine);
    if (state.history_restoring)
        return;
    if (state.history.empty() || !matches_tracked_layout(state, layout)) {
        layout_editor_history_reset(engine, layout);
        return;
    }
    if (static_cast<std::size_t>(state.history_index + 1) < state.history.size()) {
        state.history.erase(state.history.begin() + state.history_index + 1, state.history.end());
    }
    state.history.push_back(capture_snapshot(layout));
    ++state.history_index;
    if (state.history.size() > kMaxHistoryEntries) {
        state.history.erase(state.history.begin());
        --state.history_index;
    }
}

bool layout_editor_history_undo(EngineState& engine, UILayout& layout) {
    LayoutEditorState& state = layout_editor_internal::editor_state(engine);
    if (state.history.empty() || !matches_tracked_layout(state, layout))
        return false;
    if (state.history_index <= 0)
        return false;
    --state.history_index;
    state.history_restoring = true;
    apply_snapshot(layout, state.history[static_cast<std::size_t>(state.history_index)]);
    state.history_restoring = false;
    return true;
}

bool layout_editor_history_redo(EngineState& engine, UILayout& layout) {
    LayoutEditorState& state = layout_editor_internal::editor_state(engine);
    if (state.history.empty() || !matches_tracked_layout(state, layout))
        return false;
    if (static_cast<std::size_t>(state.history_index + 1) >= state.history.size())
        return false;
    ++state.history_index;
    state.history_restoring = true;
    apply_snapshot(layout, state.history[static_cast<std::size_t>(state.history_index)]);
    state.history_restoring = false;
    return true;
}

void layout_editor_history_shutdown(EngineState& engine) {
    LayoutEditorState& state = layout_editor_internal::editor_state(engine);
    state.history.clear();
    state.history_index = -1;
    state.history_restoring = false;
}
