#include "engine/layout_editor/layout_editor_panel.hpp"

#include "engine/layout_editor/layout_editor_internal.hpp"
#include "engine/layout_editor/layout_editor_history.hpp"
#include "engine/layout_editor/layout_editor.hpp"
#include "engine/layout_editor/layout_editor_interaction.hpp"

#include "engine/globals.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

using namespace layout_editor_internal;

namespace {

void sync_object_label_buffer(const UILayout& layout, int selected_index) {
    if (selected_index < 0 ||
        selected_index >= static_cast<int>(layout.objects.size())) {
        if (g_object_label_index != -1) {
            g_object_label_buffer[0] = '\0';
            g_object_label_index = -1;
        }
        return;
    }
    if (g_object_label_index == selected_index)
        return;
    const auto& obj = layout.objects[static_cast<std::size_t>(selected_index)];
    std::snprintf(g_object_label_buffer, sizeof(g_object_label_buffer), "%s",
                  obj.label.c_str());
    g_object_label_index = selected_index;
}

} // namespace

void layout_editor_render_panel(float dt) {
    if (!g_active)
        return;
    if (!ImGui::GetCurrentContext())
        return;
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.93f);
    if (!ImGui::Begin("Layout Editor", &g_active, flags)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("Ctrl+L toggle | Ctrl+S save | G snap");
    ImGui::Text("Grid %.3f (%s)",
                static_cast<double>(g_grid_step),
                g_snap_enabled ? "snap ON" : "snap OFF");
    if (!has_layouts()) {
        ImGui::TextUnformatted("No layouts loaded.");
        ImGui::End();
        return;
    }

    const char* factor_labels[] = {"Desktop", "Tablet", "Phone"};
    std::vector<const char*> labels;
    labels.reserve(es->ui_layouts_pool.size());
    static std::vector<std::string> label_storage;
    label_storage.clear();
    for (const auto& layout : es->ui_layouts_pool) {
        std::string label = layout.label + " (ID " + std::to_string(layout.id) + ") " +
                            std::to_string(layout.resolution_width) + "x" +
                            std::to_string(layout.resolution_height) + " [" +
                            factor_labels[static_cast<int>(layout.form_factor)] + "]";
        label_storage.push_back(label);
    }
    for (const auto& s : label_storage)
        labels.push_back(s.c_str());

    ImGui::Checkbox("Follow active layout", &g_follow_active_layout);
    if (ImGui::ListBox("Layouts", &g_selected_layout, labels.data(),
                       static_cast<int>(labels.size()), 6)) {
        g_follow_active_layout = false;
    }

    UILayout* layout_mut = selected_layout_mutable();
    const UILayout* layout = layout_mut;
    if (layout) {
        ImGui::Separator();
        ImGui::Text("Objects: %zu", layout->objects.size());
        static const char* kFactorLabels[] = {"Desktop", "Tablet", "Phone"};
        int form_factor = static_cast<int>(layout->form_factor);
        int width = layout->resolution_width;
        int height = layout->resolution_height;
        ImGui::Text("Layout: %dx%d", width, height);
        ImGui::SetNextItemWidth(160.0f);
        bool meta_commit = false;
        if (ImGui::InputInt("Width", &width)) {
            layout_mut->resolution_width = std::max(1, width);
            g_layout_dirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            meta_commit = true;
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::InputInt("Height", &height)) {
            layout_mut->resolution_height = std::max(1, height);
            g_layout_dirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            meta_commit = true;
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::Combo("Form factor", &form_factor, kFactorLabels, IM_ARRAYSIZE(kFactorLabels))) {
            layout_mut->form_factor = static_cast<UILayoutFormFactor>(std::clamp(form_factor, 0, 2));
            g_layout_dirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
            meta_commit = true;
        if (meta_commit)
            layout_editor_history_commit(*layout_mut);
        if (ImGui::Button("Duplicate layout")) {
            UILayout copy = *layout_mut;
            copy.id = generate_ui_layout_id();
            copy.label += "_copy";
            es->ui_layouts_pool.push_back(copy);
            layout_editor_select_single(-1);
            layout_editor_history_reset(es->ui_layouts_pool.back());
            g_selected_layout = static_cast<int>(es->ui_layouts_pool.size()) - 1;
            append_status("Layout duplicated");
        }
        ImGui::SameLine();
        if (ImGui::Button("New layout")) {
            UILayout fresh;
            fresh.id = generate_ui_layout_id();
            fresh.label = "Layout_" + std::to_string(fresh.id);
            fresh.resolution_width = gg ? static_cast<int>(gg->render_dims.x) : 1920;
            fresh.resolution_height = gg ? static_cast<int>(gg->render_dims.y) : 1080;
            fresh.form_factor = UILayoutFormFactor::Desktop;
            es->ui_layouts_pool.push_back(fresh);
            g_selected_layout = static_cast<int>(es->ui_layouts_pool.size()) - 1;
            layout_editor_clear_selection();
            layout_editor_history_reset(es->ui_layouts_pool.back());
            append_status("Layout created");
        }
    }
    if (gg) {
        glm::ivec2 dims = get_render_dimensions();
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
                bool selected = layout_editor_is_selected(i);
                if (ImGui::Selectable(entry.c_str(), selected))
                    layout_editor_select_single(i);
            }
            ImGui::EndListBox();
        }
        if (ImGui::Button("Add object")) {
            UIObject obj;
            obj.id = generate_ui_object_id();
            obj.label = "object_" + std::to_string(obj.id);
            obj.x = 0.4f;
            obj.y = 0.4f;
            obj.w = 0.2f;
            obj.h = 0.1f;
            layout_mut->objects.push_back(obj);
            layout_editor_select_single(static_cast<int>(layout_mut->objects.size()) - 1);
            g_layout_dirty = true;
            layout_editor_history_commit(*layout_mut);
        }

        int selected_obj = layout_editor_selection_count() == 1
                               ? layout_editor_primary_selection()
                               : -1;
        if (!layout_mut->objects.empty()) {
            if (selected_obj >= static_cast<int>(layout_mut->objects.size()))
                selected_obj = -1;
        } else {
            selected_obj = -1;
        }

        sync_object_label_buffer(*layout_mut, selected_obj);
        if (selected_obj >= 0 &&
            selected_obj < static_cast<int>(layout_mut->objects.size())) {
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
            if (ImGui::InputText("Label",
                                 g_object_label_buffer,
                                 sizeof(g_object_label_buffer))) {
                obj.label = g_object_label_buffer;
                g_object_label_index = selected_obj;
                changed = true;
                label_changed = true;
            }
            if (label_changed && ImGui::IsItemDeactivatedAfterEdit())
                commit_needed = true;
            ImGui::Text("Pos: x %.3f y %.3f",
                        static_cast<double>(obj.x),
                        static_cast<double>(obj.y));
            ImGui::Text("Size: w %.3f h %.3f",
                        static_cast<double>(obj.w),
                        static_cast<double>(obj.h));
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputFloat("X", &obj.x, 0.01f, 0.1f, "%.3f")) {
                obj.x = std::clamp(obj.x, 0.0f, 1.0f - obj.w);
                changed = true;
            }
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputFloat("Y", &obj.y, 0.01f, 0.1f, "%.3f")) {
                obj.y = std::clamp(obj.y, 0.0f, 1.0f - obj.h);
                changed = true;
            }
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputFloat("Width", &obj.w, 0.01f, 0.1f, "%.3f")) {
                obj.w = std::clamp(obj.w, 0.01f, 1.0f);
                obj.x = std::clamp(obj.x, 0.0f, 1.0f - obj.w);
                changed = true;
            }
            ImGui::SetNextItemWidth(list_width);
            if (ImGui::InputFloat("Height", &obj.h, 0.01f, 0.1f, "%.3f")) {
                obj.h = std::clamp(obj.h, 0.01f, 1.0f);
                obj.y = std::clamp(obj.y, 0.0f, 1.0f - obj.h);
                changed = true;
            }
            if (changed)
                g_layout_dirty = true;
            if (commit_needed)
                layout_editor_history_commit(*layout_mut);
        } else if (layout_editor_selection_count() > 1) {
            ImGui::SeparatorText("Selected objects");
            ImGui::Text("%d objects selected.", layout_editor_selection_count());
            if (ImGui::BeginTable("multi_objects", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Label");
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Pos");
                ImGui::TableSetupColumn("Size");
                ImGui::TableSetupColumn("");
                ImGui::TableHeadersRow();
                for (int index : layout_editor_selection_indices()) {
                    if (index < 0 || index >= static_cast<int>(layout_mut->objects.size()))
                        continue;
                    const auto& obj = layout_mut->objects[static_cast<std::size_t>(index)];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(obj.label.empty() ? "<unnamed>" : obj.label.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("#%d", obj.id);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("x%.3f y%.3f",
                                static_cast<double>(obj.x),
                                static_cast<double>(obj.y));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("w%.3f h%.3f",
                                static_cast<double>(obj.w),
                                static_cast<double>(obj.h));
                    ImGui::TableSetColumnIndex(4);
                    std::string btn_label = "Solo##" + std::to_string(index);
                    if (ImGui::SmallButton(btn_label.c_str()))
                        layout_editor_select_single(index);
                }
                ImGui::EndTable();
            }
        }
    }

    if (!g_status_text.empty() && g_status_timer > 0.0f) {
        g_status_timer = std::max(0.0f, g_status_timer - dt);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", g_status_text.c_str());
    }

    ImGui::End();
}
