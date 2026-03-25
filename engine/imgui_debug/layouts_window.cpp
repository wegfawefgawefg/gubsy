#include "engine/imgui_debug/windows.hpp"

#include "engine/globals.hpp"
#include "engine/ui_layouts.hpp"

#include <algorithm>
#include <imgui.h>
#include <string>
#include <vector>

void imgui_debug_render_layout_window(bool* open_flag) {
    if (!open_flag || !*open_flag)
        return;
    if (!ImGui::Begin("Debug: UI Layouts", open_flag)) {
        ImGui::End();
        return;
    }
    if (!es) {
        ImGui::TextUnformatted("Engine state unavailable.");
        ImGui::End();
        return;
    }
    if (es->ui_layouts_pool.empty()) {
        ImGui::TextUnformatted("No layouts loaded.");
        ImGui::End();
        return;
    }
    std::vector<const UILayout*> sorted;
    sorted.reserve(es->ui_layouts_pool.size());
    for (const auto& layout : es->ui_layouts_pool)
        sorted.push_back(&layout);
    std::sort(sorted.begin(), sorted.end(),
              [](const UILayout* a, const UILayout* b) {
                  if (a->label == b->label) {
                      if (a->id == b->id)
                          return a->resolution_width * a->resolution_height <
                                 b->resolution_width * b->resolution_height;
                      return a->id < b->id;
                  }
                  return a->label < b->label;
              });

    const std::string* current_label = nullptr;
    int current_id = -1;
    bool group_open = false;
    for (const UILayout* layout : sorted) {
        if (!current_label || layout->label != *current_label || layout->id != current_id) {
            if (group_open)
                ImGui::TreePop();
            current_label = &layout->label;
            current_id = layout->id;
            std::string node_id = layout->label + "_" + std::to_string(layout->id);
            std::string header = layout->label + " (Layout ID " + std::to_string(layout->id) + ")";
            group_open = ImGui::TreeNode(node_id.c_str(), "%s", header.c_str());
        }
        if (!group_open)
            continue;
        if (ImGui::TreeNode(reinterpret_cast<const void*>(layout),
                            "%dx%d", layout->resolution_width, layout->resolution_height)) {
            ImGui::Text("Objects: %zu", layout->objects.size());
            if (ImGui::BeginTable("obj_table", 5,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                                  ImVec2(0.0f, 160.0f))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Label");
                ImGui::TableSetupColumn("X");
                ImGui::TableSetupColumn("Y");
                ImGui::TableSetupColumn("Size");
                ImGui::TableHeadersRow();
                for (const auto& obj : layout->objects) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", obj.id);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(obj.label.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.3f", static_cast<double>(obj.x));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.3f", static_cast<double>(obj.y));
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%.3fx%.3f",
                                static_cast<double>(obj.w),
                                static_cast<double>(obj.h));
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
    if (group_open)
        ImGui::TreePop();
    ImGui::End();
}
