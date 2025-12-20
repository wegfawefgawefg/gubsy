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
        ImGui::Text("Layout: %dx%d",
                    layout->resolution_width, layout->resolution_height);
    }
    if (gg) {
        glm::ivec2 dims = get_render_dimensions();
        ImGui::Text("Render target: %dx%d", dims.x, dims.y);
    }

    if (layout_mut) {
        int selected_obj = layout_editor_selected_index();
        const float list_width = 320.0f;
        if (ImGui::BeginListBox("Layout objects",
                                ImVec2(list_width, 6.0f * ImGui::GetTextLineHeightWithSpacing()))) {
            for (int i = 0; i < static_cast<int>(layout_mut->objects.size()); ++i) {
                const auto& obj = layout_mut->objects[static_cast<std::size_t>(i)];
                std::string entry = obj.label.empty()
                                        ? ("#" + std::to_string(obj.id))
                                        : (obj.label + " (#" + std::to_string(obj.id) + ")");
                bool selected = (i == selected_obj);
                if (ImGui::Selectable(entry.c_str(), selected))
                    layout_editor_select(i);
            }
            ImGui::EndListBox();
        }

        selected_obj = layout_editor_selected_index();
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
            if (changed)
                g_layout_dirty = true;
            if (commit_needed)
                layout_editor_history_commit(*layout_mut);
        }
    }

    if (!g_status_text.empty() && g_status_timer > 0.0f) {
        g_status_timer = std::max(0.0f, g_status_timer - dt);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", g_status_text.c_str());
    }

    ImGui::End();
}
