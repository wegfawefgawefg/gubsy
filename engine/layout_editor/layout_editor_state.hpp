#pragma once

#include "engine/layout_editor/layout_editor_interaction.hpp"
#include "engine/ui_layouts.hpp"

#include <glayout/editor.hpp>
#include <string>

namespace layout_editor_internal {

struct PendingLayoutRequest {
    bool valid{false};
    int id{-1};
    int width{0};
    int height{0};
};

struct LayoutEditorState {
    LayoutEditorState() {
        editor.grid_step = 0.2f;
        editor.snap_enabled = true;
    }

    bool active{false};
    int selected_layout{0};
    std::string status_text{};
    float status_timer{0.0f};
    bool follow_active_layout{true};
    char object_label_buffer[128]{};
    int object_label_index{-1};
    bool history_initialized{false};
    int history_layout_id{-1};
    int history_layout_width{0};
    int history_layout_height{0};
    bool layout_dirty{false};
    PendingLayoutRequest last_request{};
    LayoutEditorViewport viewport{};
    glayout::EditorState editor{};
};

} // namespace layout_editor_internal
