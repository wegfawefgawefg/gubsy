#include "src/engine_state.hpp"
#include "src/imgui_debug/windows.hpp"
#include "src/ui_layouts.hpp"

#include <algorithm>
#include <imgui.h>
#include <string>
#include <vector>

void imgui_debug_render_layout_window(EngineState& engine, bool* open_flag) {
    if (!open_flag || !*open_flag)
        return;
    if (!ImGui::Begin("Debug: UI Layouts", open_flag)) {
        ImGui::End();
        return;
    }
    if (engine.ui_layouts.layouts.empty()) {
        ImGui::TextUnformatted("No layouts loaded.");
        ImGui::End();
        return;
    }
    std::vector<const UILayout*> sorted;
    sorted.reserve(engine.ui_layouts.layouts.size());
    for (const auto& layout : engine.ui_layouts.layouts)
        sorted.push_back(&layout);
    std::sort(sorted.begin(), sorted.end(), [](const UILayout* a, const UILayout* b) {
        if (a->label == b->label) {
            if (a->id == b->id)
                return a->width * a->height < b->width * b->height;
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
        if (ImGui::TreeNode(reinterpret_cast<const void*>(layout), "%dx%d", layout->width,
                            layout->height)) {
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
                    ImGui::Text("%.3f", static_cast<double>(obj.rect.x));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.3f", static_cast<double>(obj.rect.y));
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%.3fx%.3f", static_cast<double>(obj.rect.w),
                                static_cast<double>(obj.rect.h));
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
