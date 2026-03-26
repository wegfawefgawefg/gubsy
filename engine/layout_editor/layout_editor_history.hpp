#pragma once

#include <cstddef>

struct EngineState;
struct UILayout;

// Resets the undo stack to track the provided layout's current state.
void layout_editor_history_reset(EngineState& engine, const UILayout& layout);

// Records the current state of the tracked layout as a new undo entry.
void layout_editor_history_commit(EngineState& engine, const UILayout& layout);

// Applies the previous snapshot. Returns true if state changed.
bool layout_editor_history_undo(EngineState& engine, UILayout& layout);

// Applies the next snapshot. Returns true if state changed.
bool layout_editor_history_redo(EngineState& engine, UILayout& layout);

// Clears history state (called on shutdown).
void layout_editor_history_shutdown(EngineState& engine);
