#pragma once

#include <string>

struct UILayout;

namespace layout_editor_internal {

extern bool g_active;
extern int g_selected_layout;
extern float g_grid_step;
extern bool g_snap_enabled;
extern std::string g_status_text;
extern float g_status_timer;
extern bool g_follow_active_layout;
extern bool g_mouse_was_down;
extern char g_object_label_buffer[128];
extern int g_object_label_index;
extern bool g_drag_dirty;
extern bool g_history_initialized;
extern int g_history_layout_id;
extern int g_history_layout_width;
extern int g_history_layout_height;
extern bool g_layout_dirty;

bool has_layouts();
UILayout* selected_layout_mutable();
const UILayout* selected_layout();
void append_status(const std::string& text);

} // namespace layout_editor_internal
