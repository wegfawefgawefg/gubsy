#include "glayout/imgui.hpp"

#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <string>

namespace glayout::imgui {
namespace {

const char* form_factor_label(FormFactor form_factor) {
    switch (form_factor) {
    case FormFactor::Desktop:
        return "Desktop";
    case FormFactor::Tablet:
        return "Tablet";
    case FormFactor::Phone:
        return "Phone";
    }
    return "Desktop";
}

std::string layout_list_label(const Layout& layout) {
    return layout.label + " #" + std::to_string(layout.id) + " " + std::to_string(layout.width) +
           "x" + std::to_string(layout.height) + " " +
           std::string(form_factor_label(layout.form_factor));
}

void set_status(EditorState& editor, const char* text) {
    editor.status_text = text;
}

void commit_on_activation(EditorState& editor, const Layout& layout) {
    if (ImGui::IsItemActivated())
        editor_commit_undo(editor, layout);
}

void commit_after_edit(EditorState& editor, const Layout& layout) {
    if (ImGui::IsItemDeactivatedAfterEdit())
        editor_commit_undo(editor, layout);
}

bool input_text_string(EditorState& editor, const Layout& layout, const char* label,
                       std::string& value) {
    char buffer[128]{};
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    bool changed = ImGui::InputText(label, buffer, sizeof(buffer));
    commit_on_activation(editor, layout);
    if (changed)
        value = buffer;
    commit_after_edit(editor, layout);
    return changed;
}

void render_object_table(const Layout& layout, const char* table_id) {
    if (!ImGui::BeginTable(table_id, 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        return;

    ImGui::TableSetupColumn("ID");
    ImGui::TableSetupColumn("Label");
    ImGui::TableSetupColumn("X/Y");
    ImGui::TableSetupColumn("W/H");
    ImGui::TableSetupColumn("Index");
    ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < layout.objects.size(); ++i) {
        const Object& object = layout.objects[i];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", object.id);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(object.label.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.3f, %.3f", static_cast<double>(object.rect.x),
                    static_cast<double>(object.rect.y));
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.3f, %.3f", static_cast<double>(object.rect.w),
                    static_cast<double>(object.rect.h));
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%d", static_cast<int>(i));
    }

    ImGui::EndTable();
}

bool render_layout_list(EditorState& editor, std::vector<Layout>& layouts,
                        int& selected_layout_index, float height) {
    bool changed_selection = false;
    if (!ImGui::BeginListBox("Layouts", ImVec2(-FLT_MIN, height)))
        return false;

    for (int i = 0; i < static_cast<int>(layouts.size()); ++i) {
        const Layout& layout = layouts[static_cast<std::size_t>(i)];
        std::string label = layout_list_label(layout);
        if (ImGui::Selectable(label.c_str(), selected_layout_index == i)) {
            selected_layout_index = i;
            editor_clear_selection(editor);
            changed_selection = true;
        }
    }

    ImGui::EndListBox();
    return changed_selection;
}

bool render_layout_actions(EditorState& editor, std::vector<Layout>& layouts,
                           int& selected_layout_index) {
    bool changed = false;
    if (layouts.empty()) {
        if (ImGui::Button("New layout")) {
            Layout layout;
            layout.id = generate_layout_id(layouts);
            layout.label = "Layout_" + std::to_string(layout.id);
            layout.width = 1920;
            layout.height = 1080;
            layout.form_factor = FormFactor::Desktop;
            layouts.push_back(layout);
            selected_layout_index = 0;
            editor.dirty = true;
            set_status(editor, "Layout created");
            changed = true;
        }
        return changed;
    }

    selected_layout_index =
        std::clamp(selected_layout_index, 0, static_cast<int>(layouts.size()) - 1);
    Layout& selected = layouts[static_cast<std::size_t>(selected_layout_index)];

    if (ImGui::Button("Duplicate layout")) {
        Layout copy = selected;
        copy.id = generate_layout_id(layouts);
        copy.label += "_copy";
        layouts.push_back(copy);
        selected_layout_index = static_cast<int>(layouts.size()) - 1;
        editor_clear_selection(editor);
        editor.dirty = true;
        set_status(editor, "Layout duplicated");
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("New layout")) {
        Layout layout;
        layout.id = generate_layout_id(layouts);
        layout.label = "Layout_" + std::to_string(layout.id);
        layout.width = selected.width;
        layout.height = selected.height;
        layout.form_factor = selected.form_factor;
        layouts.push_back(layout);
        selected_layout_index = static_cast<int>(layouts.size()) - 1;
        editor_clear_selection(editor);
        editor.dirty = true;
        set_status(editor, "Layout created");
        changed = true;
    }

    return changed;
}

bool render_object_editor(EditorState& editor, Layout& layout) {
    bool changed = false;

    if (ImGui::Button("Add object")) {
        Object object;
        object.id = generate_object_id(layout);
        object.label = "object_" + std::to_string(object.id);
        object.rect = Rect{0.4f, 0.4f, 0.2f, 0.1f};
        layout.objects.push_back(object);
        editor_select_single(editor, static_cast<int>(layout.objects.size()) - 1);
        editor.dirty = true;
        set_status(editor, "Object added");
        editor_commit_undo(editor, layout);
        changed = true;
    }

    if (ImGui::BeginListBox("Objects", ImVec2(-FLT_MIN, 128.0f))) {
        for (int i = 0; i < static_cast<int>(layout.objects.size()); ++i) {
            const Object& object = layout.objects[static_cast<std::size_t>(i)];
            std::string label = object.label + " #" + std::to_string(object.id);
            if (ImGui::Selectable(label.c_str(), editor_is_selected(editor, i)))
                editor_select_single(editor, i);
        }
        ImGui::EndListBox();
    }

    if (editor.primary >= 0 && editor.primary < static_cast<int>(layout.objects.size())) {
        Object& object = layout.objects[static_cast<std::size_t>(editor.primary)];
        ImGui::SeparatorText("Selected object");
        ImGui::SetNextItemWidth(120.0f);
        int object_id = object.id;
        bool id_changed = ImGui::InputInt("ID", &object_id);
        commit_on_activation(editor, layout);
        if (id_changed) {
            object.id = object_id;
            editor.dirty = true;
            changed = true;
        }
        commit_after_edit(editor, layout);
        if (input_text_string(editor, layout, "Label", object.label)) {
            editor.dirty = true;
            changed = true;
        }
        ImGui::Text("Pos: x %.3f y %.3f", static_cast<double>(object.rect.x),
                    static_cast<double>(object.rect.y));
        ImGui::Text("Size: w %.3f h %.3f", static_cast<double>(object.rect.w),
                    static_cast<double>(object.rect.h));

        bool rect_changed = false;
        ImGui::SetNextItemWidth(320.0f);
        if (ImGui::InputFloat("X", &object.rect.x, 0.01f, 0.1f, "%.3f")) {
            object.rect.x = std::clamp(object.rect.x, 0.0f, 1.0f - object.rect.w);
            rect_changed = true;
        }
        commit_on_activation(editor, layout);
        commit_after_edit(editor, layout);
        ImGui::SetNextItemWidth(320.0f);
        if (ImGui::InputFloat("Y", &object.rect.y, 0.01f, 0.1f, "%.3f")) {
            object.rect.y = std::clamp(object.rect.y, 0.0f, 1.0f - object.rect.h);
            rect_changed = true;
        }
        commit_on_activation(editor, layout);
        commit_after_edit(editor, layout);
        ImGui::SetNextItemWidth(320.0f);
        if (ImGui::InputFloat("Width", &object.rect.w, 0.01f, 0.1f, "%.3f")) {
            object.rect.w = std::clamp(object.rect.w, 0.01f, 1.0f);
            object.rect.x = std::clamp(object.rect.x, 0.0f, 1.0f - object.rect.w);
            rect_changed = true;
        }
        commit_on_activation(editor, layout);
        commit_after_edit(editor, layout);
        ImGui::SetNextItemWidth(320.0f);
        if (ImGui::InputFloat("Height", &object.rect.h, 0.01f, 0.1f, "%.3f")) {
            object.rect.h = std::clamp(object.rect.h, 0.01f, 1.0f);
            object.rect.y = std::clamp(object.rect.y, 0.0f, 1.0f - object.rect.h);
            rect_changed = true;
        }
        commit_on_activation(editor, layout);
        commit_after_edit(editor, layout);
        if (rect_changed) {
            editor.dirty = true;
            changed = true;
        }
    }

    if (editor.selection.size() > 1) {
        ImGui::SeparatorText("Selection");
        Rect bounds;
        if (editor_selection_bounds(editor, layout, bounds)) {
            ImGui::Text("Bounds: %.3f %.3f %.3f %.3f", static_cast<double>(bounds.x),
                        static_cast<double>(bounds.y), static_cast<double>(bounds.w),
                        static_cast<double>(bounds.h));
        }
        if (ImGui::BeginTable("selected_objects", 6,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("X/Y");
            ImGui::TableSetupColumn("W/H");
            ImGui::TableSetupColumn("Index");
            ImGui::TableSetupColumn("");
            ImGui::TableHeadersRow();
            for (int index : editor.selection) {
                if (index < 0 || index >= static_cast<int>(layout.objects.size()))
                    continue;
                const Object& object = layout.objects[static_cast<std::size_t>(index)];
                ImGui::PushID(index);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", object.id);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(object.label.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f, %.3f", static_cast<double>(object.rect.x),
                            static_cast<double>(object.rect.y));
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f, %.3f", static_cast<double>(object.rect.w),
                            static_cast<double>(object.rect.h));
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%d", index);
                ImGui::TableSetColumnIndex(5);
                if (ImGui::SmallButton("Solo")) {
                    editor_select_single(editor, index);
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    return changed;
}

bool render_layout_metadata(EditorState& editor, Layout& layout) {
    bool changed = false;
    ImGui::SeparatorText("Layout");

    ImGui::Text("Layout: %dx%d", layout.width, layout.height);
    ImGui::SetNextItemWidth(320.0f);
    int layout_id = layout.id;
    bool id_changed = ImGui::InputInt("Layout ID", &layout_id);
    commit_on_activation(editor, layout);
    if (id_changed) {
        layout.id = layout_id;
        editor.dirty = true;
        changed = true;
    }
    commit_after_edit(editor, layout);
    ImGui::SetNextItemWidth(320.0f);
    if (input_text_string(editor, layout, "Layout label", layout.label)) {
        editor.dirty = true;
        changed = true;
    }

    int width = layout.width;
    ImGui::SetNextItemWidth(160.0f);
    bool width_changed = ImGui::InputInt("Width", &width);
    commit_on_activation(editor, layout);
    if (width_changed) {
        layout.width = std::max(1, width);
        editor.dirty = true;
        changed = true;
    }
    commit_after_edit(editor, layout);

    int height = layout.height;
    ImGui::SetNextItemWidth(160.0f);
    bool height_changed = ImGui::InputInt("Height", &height);
    commit_on_activation(editor, layout);
    if (height_changed) {
        layout.height = std::max(1, height);
        editor.dirty = true;
        changed = true;
    }
    commit_after_edit(editor, layout);

    int form_factor = static_cast<int>(layout.form_factor);
    bool form_changed = ImGui::Combo("Form factor", &form_factor, "Desktop\0Tablet\0Phone\0");
    commit_on_activation(editor, layout);
    if (form_changed) {
        layout.form_factor = static_cast<FormFactor>(std::clamp(form_factor, 0, 2));
        editor.dirty = true;
        changed = true;
    }
    commit_after_edit(editor, layout);

    return changed;
}

} // namespace

void render_layout_browser(const std::vector<Layout>& layouts) {
    if (!ImGui::Begin("glayout: Layouts")) {
        ImGui::End();
        return;
    }

    if (layouts.empty()) {
        ImGui::TextUnformatted("No layouts loaded.");
        ImGui::End();
        return;
    }

    for (const Layout& layout : layouts) {
        std::string title = layout_list_label(layout);
        if (!ImGui::TreeNode(title.c_str()))
            continue;

        ImGui::Text("Objects: %zu", layout.objects.size());
        render_object_table(layout, "objects");

        ImGui::TreePop();
    }

    ImGui::End();
}

bool render_editor_panel(EditorState& editor, Layout& layout) {
    bool changed = false;

    if (!ImGui::Begin("glayout: Editor")) {
        ImGui::End();
        return false;
    }

    ImGui::Text("Layout #%d: %s", layout.id, layout.label.c_str());
    ImGui::Text("%dx%d %s", layout.width, layout.height, form_factor_label(layout.form_factor));
    ImGui::Checkbox("Snap", &editor.snap_enabled);
    ImGui::DragFloat("Grid", &editor.grid_step, 0.005f, 0.001f, 1.0f, "%.3f");

    if (ImGui::Button("Save")) {
        editor.save_requested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo")) {
        bool undone = editor_undo(editor, layout);
        editor.dirty = undone || editor.dirty;
        changed = undone || changed;
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo")) {
        bool redone = editor_redo(editor, layout);
        editor.dirty = redone || editor.dirty;
        changed = redone || changed;
    }

    if (editor.dirty)
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Dirty");

    changed = render_layout_metadata(editor, layout) || changed;
    changed = render_object_editor(editor, layout) || changed;

    ImGui::End();
    return changed;
}

bool render_layout_pool_editor(EditorState& editor, std::vector<Layout>& layouts,
                               int& selected_layout_index) {
    bool changed = false;

    if (!ImGui::Begin("glayout: Layout Pool")) {
        ImGui::End();
        return false;
    }

    if (layouts.empty()) {
        ImGui::TextUnformatted("No layouts loaded.");
        if (ImGui::Button("New layout")) {
            Layout layout;
            layout.id = generate_layout_id(layouts);
            layout.label = "Layout_" + std::to_string(layout.id);
            layout.width = 1920;
            layout.height = 1080;
            layout.form_factor = FormFactor::Desktop;
            layouts.push_back(layout);
            selected_layout_index = 0;
            editor.dirty = true;
            changed = true;
        }
        ImGui::End();
        return changed;
    }

    selected_layout_index =
        std::clamp(selected_layout_index, 0, static_cast<int>(layouts.size()) - 1);

    changed = render_layout_list(editor, layouts, selected_layout_index, 128.0f) || changed;
    changed = render_layout_actions(editor, layouts, selected_layout_index) || changed;

    ImGui::End();

    if (selected_layout_index >= 0 && selected_layout_index < static_cast<int>(layouts.size())) {
        changed =
            render_editor_panel(editor, layouts[static_cast<std::size_t>(selected_layout_index)]) ||
            changed;
    }

    return changed;
}

bool render_integrated_editor(EditorState& editor, std::vector<Layout>& layouts,
                              int& selected_layout_index) {
    bool changed = false;

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.93f);
    if (!ImGui::Begin("Layout Editor", nullptr, flags)) {
        ImGui::End();
        return false;
    }

    ImGui::TextUnformatted("Ctrl+L toggle | Ctrl+S save | G snap");
    ImGui::Text("Grid %.3f (%s)", static_cast<double>(editor.grid_step),
                editor.snap_enabled ? "snap ON" : "snap OFF");

    if (layouts.empty()) {
        ImGui::TextUnformatted("No layouts loaded.");
        changed = render_layout_actions(editor, layouts, selected_layout_index) || changed;
        ImGui::End();
        return changed;
    }

    selected_layout_index =
        std::clamp(selected_layout_index, 0, static_cast<int>(layouts.size()) - 1);
    Layout& selected = layouts[static_cast<std::size_t>(selected_layout_index)];

    if (ImGui::Button("Save"))
        editor.save_requested = true;
    ImGui::SameLine();
    if (ImGui::Button("Undo")) {
        bool undone = editor_undo(editor, selected);
        editor.dirty = undone || editor.dirty;
        changed = undone || changed;
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo")) {
        bool redone = editor_redo(editor, selected);
        editor.dirty = redone || editor.dirty;
        changed = redone || changed;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &editor.snap_enabled);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::DragFloat("Grid", &editor.grid_step, 0.005f, 0.001f, 1.0f, "%.3f");

    ImGui::Separator();
    changed = render_layout_list(editor, layouts, selected_layout_index,
                                 6.0f * ImGui::GetTextLineHeightWithSpacing()) ||
              changed;
    changed = render_layout_actions(editor, layouts, selected_layout_index) || changed;
    selected_layout_index =
        std::clamp(selected_layout_index, 0, static_cast<int>(layouts.size()) - 1);

    Layout& current = layouts[static_cast<std::size_t>(selected_layout_index)];
    ImGui::Separator();
    ImGui::Text("Objects: %zu", current.objects.size());
    changed = render_layout_metadata(editor, current) || changed;
    changed = render_object_editor(editor, current) || changed;

    if (!editor.status_text.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", editor.status_text.c_str());
    }

    ImGui::End();
    return changed;
}

} // namespace glayout::imgui
