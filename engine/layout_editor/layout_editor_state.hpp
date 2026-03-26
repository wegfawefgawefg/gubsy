#pragma once

#include "engine/ui_layouts.hpp"

#include <string>
#include <vector>

namespace layout_editor_internal {

struct PendingLayoutRequest {
    bool valid{false};
    int id{-1};
    int width{0};
    int height{0};
};

struct LayoutClipboard {
    std::vector<UIObject> objects;
};

struct LayoutEditorState {
    bool active{false};
    int selected_layout{0};
    float grid_step{0.2f};
    bool snap_enabled{true};
    std::string status_text{};
    float status_timer{0.0f};
    bool follow_active_layout{true};
    bool mouse_was_down{false};
    char object_label_buffer[128]{};
    int object_label_index{-1};
    bool drag_dirty{false};
    bool history_initialized{false};
    int history_layout_id{-1};
    int history_layout_width{0};
    int history_layout_height{0};
    bool layout_dirty{false};
    PendingLayoutRequest last_request{};
    LayoutClipboard clipboard{};
};

} // namespace layout_editor_internal
