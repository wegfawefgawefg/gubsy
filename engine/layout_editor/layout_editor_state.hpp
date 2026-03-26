#pragma once

#include "engine/layout_editor/layout_editor_interaction.hpp"
#include "engine/ui_layouts.hpp"

#include <string>
#include <vector>

namespace layout_editor_internal {

struct GroupMember {
    int index{-1};
    float start_x{0.0f};
    float start_y{0.0f};
    float start_w{0.0f};
    float start_h{0.0f};
    float rel_x{0.0f};
    float rel_y{0.0f};
    float rel_w{0.0f};
    float rel_h{0.0f};
};

struct DragState {
    bool active{false};
    int object_index{-1};
    HandleType handle{HandleType::Center};
    bool group{false};
    float offset_x{0.0f};
    float offset_y{0.0f};
    float start_x{0.0f};
    float start_y{0.0f};
    float start_w{0.0f};
    float start_h{0.0f};
    float group_start_x{0.0f};
    float group_start_y{0.0f};
    float group_start_w{0.0f};
    float group_start_h{0.0f};
    std::vector<GroupMember> members;
    std::vector<float> snap_edges_x;
    std::vector<float> snap_edges_y;
};

struct LayoutSnapshot {
    int layout_id{0};
    int width{0};
    int height{0};
    std::vector<UIObject> objects;
};

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
    LayoutEditorViewport viewport{};
    DragState drag{};
    std::vector<int> selection{};
    int primary{-1};
    std::vector<LayoutSnapshot> history{};
    int history_index{-1};
    bool history_restoring{false};
};

} // namespace layout_editor_internal
