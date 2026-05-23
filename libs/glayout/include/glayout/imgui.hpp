#pragma once

#include "glayout/editor.hpp"
#include "glayout/layout.hpp"

#include <vector>

namespace glayout::imgui {

void render_layout_browser(const std::vector<Layout>& layouts);
bool render_editor_panel(EditorState& editor, Layout& layout);
bool render_layout_pool_editor(EditorState& editor, std::vector<Layout>& layouts,
                               int& selected_layout_index);
bool render_integrated_editor(EditorState& editor, std::vector<Layout>& layouts,
                              int& selected_layout_index);

inline bool render_layout_pool_editor(EditorState& editor, LayoutStore& store,
                                      int& selected_layout_index) {
    return render_layout_pool_editor(editor, store.layouts, selected_layout_index);
}

inline bool render_integrated_editor(EditorState& editor, LayoutStore& store,
                                     int& selected_layout_index) {
    return render_integrated_editor(editor, store.layouts, selected_layout_index);
}

} // namespace glayout::imgui
