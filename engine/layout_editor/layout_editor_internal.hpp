#pragma once

#include "engine/engine_state.hpp"
#include "engine/layout_editor/layout_editor_state.hpp"
#include "engine/ui_layouts.hpp"

#include <string>

namespace layout_editor_internal {

LayoutEditorState& editor_state(EngineState& engine);
const LayoutEditorState& editor_state(const EngineState& engine);

bool has_layouts(const EngineState& engine);
UILayout* selected_layout_mutable(EngineState& engine);
const UILayout* selected_layout(EngineState& engine);
void append_status(EngineState& engine, const std::string& text);

} // namespace layout_editor_internal
