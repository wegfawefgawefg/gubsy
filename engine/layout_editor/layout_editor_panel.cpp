#include "engine/layout_editor/layout_editor_panel.hpp"

#include "engine/engine_state.hpp"
#include "engine/graphics.hpp"
#include "engine/layout_editor/layout_editor.hpp"
#include "engine/layout_editor/layout_editor_history.hpp"
#include "engine/layout_editor/layout_editor_interaction.hpp"
#include "engine/layout_editor/layout_editor_internal.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"

#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <string>
#include <vector>

using namespace layout_editor_internal;

namespace {

void sync_object_label_buffer(LayoutEditorState& state, const UILayout& layout,
                              int selected_index) {
    if (selected_index < 0 || selected_index >= static_cast<int>(layout.objects.size())) {
        if (state.object_label_index != -1) {
            state.object_label_buffer[0] = '\0';
            state.object_label_index = -1;
        }
        return;
    }
    if (state.object_label_index == selected_index)
        return;
    const auto& obj = layout.objects[static_cast<std::size_t>(selected_index)];
    std::snprintf(state.object_label_buffer, sizeof(state.object_label_buffer), "%s",
                  obj.label.c_str());
    state.object_label_index = selected_index;
}

} // namespace

void layout_editor_render_panel(EngineState& engine, float dt) {
    LayoutEditorState& state = editor_state(engine);
    (void)dt;
    if (!state.active)
        return;
    if (!ImGui::GetCurrentContext())
        return;
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.93f);
    if (!ImGui::Begin("Layout Editor", &state.active, flags)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("Ctrl+L toggle | Ctrl+S save | G snap");
    ImGui::Text("Grid %.3f (%s)", static_cast<double>(state.editor.grid_step),
                state.editor.snap_enabled ? "snap ON" : "snap OFF");
    if (!has_layouts(engine)) {
        ImGui::TextUnformatted("No layouts loaded.");
        ImGui::End();
        return;
    }

    const char* factor_labels[] = {"Desktop", "Tablet", "Phone"};
    std::vector<const char*> labels;
    labels.reserve(engine.ui_layouts.layouts.size());
    static std::vector<std::string> label_storage;
    label_storage.clear();
    for (const auto& layout : engine.ui_layouts.layouts) {
        std::string label = layout.label + " (ID " + std::to_string(layout.id) + ") " +
                            std::to_string(layout.width) + "x" + std::to_string(layout.height) +
                            " [" + factor_labels[static_cast<int>(layout.form_factor)] + "]";
        label_storage.push_back(label);
    }
    for (const auto& s : label_storage)
        labels.push_back(s.c_str());

    ImGui::Checkbox("Follow active layout", &state.follow_active_layout);
    if (ImGui::ListBox("Layouts", &state.selected_layout, labels.data(),
                       static_cast<int>(labels.size()), 6)) {
        state.follow_active_layout = false;
    }

    UILayout* layout_mut = selected_layout_mutable(engine);
    const UILayout* layout = layout_mut;
    if (layout) {
        ImGui::Separator();
        ImGui::Text("Objects: %zu", layout->objects.size());
        static const char* kFactorLabels[] = {"Desktop", "Tablet", "Phone"};
        int form_factor = static_cast<int>(layout->form_factor);
        int width = layout->width;
        int height = layout->height;
        ImGui::Text("Layout: %dx%d", width, height);
        ImGui::SetNextItemWidth(160.0f);
        bool meta_commit = false;
        if (ImGui::InputInt("Width", &width)) {
            layout_mut->width = std::max(1, width);
            state.layout_dirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            meta_commit = true;
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::InputInt("Height", &height)) {
            layout_mut->height = std::max(1, height);
            state.layout_dirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            meta_commit = true;
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::Combo("Form factor", &form_factor, kFactorLabels, IM_ARRAYSIZE(kFactorLabels))) {
            layout_mut->form_factor =
                static_cast<UILayoutFormFactor>(std::clamp(form_factor, 0, 2));
            state.layout_dirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            meta_commit = true;
        if (meta_commit)
            layout_editor_history_commit(engine, *layout_mut);
        if (ImGui::Button("Duplicate layout")) {
            UILayout copy = *layout_mut;
            copy.id = generate_ui_layout_id();
            copy.label += "_copy";
            engine.ui_layouts.layouts.push_back(copy);
            layout_editor_select_single(engine, -1);
            layout_editor_history_reset(engine, engine.ui_layouts.layouts.back());
            state.selected_layout = static_cast<int>(engine.ui_layouts.layouts.size()) - 1;
            append_status(engine, "Layout duplicated");
        }
        ImGui::SameLine();
        if (ImGui::Button("New layout")) {
            Graphics* graphics = current_graphics(engine);
            UILayout fresh;
            fresh.id = generate_ui_layout_id();
            fresh.label = "Layout_" + std::to_string(fresh.id);
            fresh.width = graphics ? static_cast<int>(graphics->render_dims.x) : 1920;
            fresh.height = graphics ? static_cast<int>(graphics->render_dims.y) : 1080;
            fresh.form_factor = UILayoutFormFactor::Desktop;
            engine.ui_layouts.layouts.push_back(fresh);
            state.selected_layout = static_cast<int>(engine.ui_layouts.layouts.size()) - 1;
            layout_editor_clear_selection(engine);
            layout_editor_history_reset(engine, engine.ui_layouts.layouts.back());
            append_status(engine, "Layout created");
        }
    }
    Graphics* graphics = current_graphics(engine);
    if (graphics) {
        glm::ivec2 dims = get_render_dimensions(engine);
        ImGui::Text("Render target: %dx%d", dims.x, dims.y);
    }

    if (layout_mut) {
        const float list_width = 320.0f;
        if (ImGui::BeginListBox("Layout objects",
                                ImVec2(list_width, 6.0f * ImGui::GetTextLineHeightWithSpacing()))) {
            for (int i = 0; i < static_cast<int>(layout_mut->objects.size()); ++i) {
                const auto& obj = layout_mut->objects[static_cast<std::size_t>(i)];
                std::string entry = obj.label.empty()
                                        ? ("#" + std::to_string(obj.id))
                                        : (obj.label + " (#" + std::to_string(obj.id) + ")");
                bool selected = layout_editor_is_selected(engine, i);
                if (ImGui::Selectable(entry.c_str(), selected))
                    layout_editor_select_single(engine, i);
            }
            ImGui::EndListBox();
        }
        if (ImGui::Button("Add object")) {
            UIObject obj;
            obj.id = generate_ui_object_id();
            obj.label = "object_" + std::to_string(obj.id);
            obj.rect.x = 0.4f;
            obj.rect.y = 0.4f;
            obj.rect.w = 0.2f;
            obj.rect.h = 0.1f;
            layout_mut->objects.push_back(obj);
            layout_editor_select_single(engine, static_cast<int>(layout_mut->objects.size()) - 1);
            state.layout_dirty = true;
            layout_editor_history_commit(engine, *layout_mut);
        }

        int selected_obj = layout_editor_selection_count(engine) == 1
                               ? layout_editor_primary_selection(engine)
                               : -1;
        if (!layout_mut->objects.empty()) {
            if (selected_obj >= static_cast<int>(layout_mut->objects.size()))
                selected_obj = -1;
        } else {
            selected_obj = -1;
        }

        sync_object_label_buffer(state, *layout_mut, selected_obj);
        if (selected_obj >= 0 && selected_obj < static_cast<int>(layout_mut->objects.size())) {
            ImGui::SeparatorText("Selected object");
            auto& obj = layout_mut->objects[static_cast<std::size_t>(selected_obj)];
            bool changed = false;
            bool commit_needed = false;
            bool id_changed = false;
            int obj_id = obj.id;
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputInt("Object ID", &obj_id)) {
                obj.id = obj_id;
                changed = true;
                id_changed = true;
            }
            if (id_changed && ImGui::IsItemDeactivatedAfterEdit())
                commit_needed = true;
            ImGui::SetNextItemWidth(list_width);
            bool label_changed = false;
            if (ImGui::InputText("Label", state.object_label_buffer,
                                 sizeof(state.object_label_buffer))) {
                obj.label = state.object_label_buffer;
                state.object_label_index = selected_obj;
                changed = true;
                label_changed = true;
            }
            if (label_changed && ImGui::IsItemDeactivatedAfterEdit())
                commit_needed = true;
            ImGui::Text("Pos: x %.3f y %.3f", static_cast<double>(obj.rect.x),
                        static_cast<double>(obj.rect.y));
            ImGui::Text("Size: w %.3f h %.3f", static_cast<double>(obj.rect.w),
                        static_cast<double>(obj.rect.h));
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputFloat("X", &obj.rect.x, 0.01f, 0.1f, "%.3f")) {
                obj.rect.x = std::clamp(obj.rect.x, 0.0f, 1.0f - obj.rect.w);
                changed = true;
            }
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputFloat("Y", &obj.rect.y, 0.01f, 0.1f, "%.3f")) {
                obj.rect.y = std::clamp(obj.rect.y, 0.0f, 1.0f - obj.rect.h);
                changed = true;
            }
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputFloat("Width", &obj.rect.w, 0.01f, 0.1f, "%.3f")) {
                obj.rect.w = std::clamp(obj.rect.w, 0.01f, 1.0f);
                obj.rect.x = std::clamp(obj.rect.x, 0.0f, 1.0f - obj.rect.w);
                changed = true;
            }
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputFloat("Height", &obj.rect.h, 0.01f, 0.1f, "%.3f")) {
                obj.rect.h = std::clamp(obj.rect.h, 0.01f, 1.0f);
                obj.rect.y = std::clamp(obj.rect.y, 0.0f, 1.0f - obj.rect.h);
                changed = true;
            }
            if (changed)
                state.layout_dirty = true;
            if (commit_needed)
                layout_editor_history_commit(engine, *layout_mut);
        } else if (layout_editor_selection_count(engine) > 1) {
            ImGui::SeparatorText("Selected objects");
            ImGui::Text("%d objects selected.", layout_editor_selection_count(engine));
            if (ImGui::BeginTable("multi_objects", 5,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Label");
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Pos");
                ImGui::TableSetupColumn("Size");
                ImGui::TableSetupColumn("");
                ImGui::TableHeadersRow();
                for (int index : layout_editor_selection_indices(engine)) {
                    if (index < 0 || index >= static_cast<int>(layout_mut->objects.size()))
                        continue;
                    const auto& obj = layout_mut->objects[static_cast<std::size_t>(index)];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(obj.label.empty() ? "<unnamed>" : obj.label.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("#%d", obj.id);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("x%.3f y%.3f", static_cast<double>(obj.rect.x),
                                static_cast<double>(obj.rect.y));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("w%.3f h%.3f", static_cast<double>(obj.rect.w),
                                static_cast<double>(obj.rect.h));
                    ImGui::TableSetColumnIndex(4);
                    std::string btn_label = "Solo##" + std::to_string(index);
                    if (ImGui::SmallButton(btn_label.c_str()))
                        layout_editor_select_single(engine, index);
                }
                ImGui::EndTable();
            }
        }
    }

    if (!state.status_text.empty() && state.status_timer > 0.0f) {
        state.status_timer = std::max(0.0f, state.status_timer - dt);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", state.status_text.c_str());
    }

    ImGui::End();
}
