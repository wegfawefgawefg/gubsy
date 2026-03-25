#include "engine/imgui_debug/windows.hpp"

#include "engine/globals.hpp"
#include "engine/binds_profiles.hpp"

#include <imgui.h>

void imgui_debug_render_binds_window(bool* open_flag) {
    if (!open_flag || !*open_flag)
        return;
    if (!ImGui::Begin("Debug: Binds Profiles", open_flag)) {
        ImGui::End();
        return;
    }
    if (!es) {
        ImGui::TextUnformatted("Engine state unavailable.");
        ImGui::End();
        return;
    }
    if (es->binds_profiles.empty()) {
        ImGui::TextUnformatted("No binds profiles loaded.");
        ImGui::End();
        return;
    }
    for (const auto& profile : es->binds_profiles) {
        if (ImGui::TreeNode(reinterpret_cast<const void*>(static_cast<intptr_t>(profile.id)),
                            "%s (ID %d)", profile.name.c_str(), profile.id)) {
            ImGui::Text("Button binds: %zu", profile.button_binds.size());
            ImGui::Text("1D analog binds: %zu", profile.analog_1d_binds.size());
            ImGui::Text("2D analog binds: %zu", profile.analog_2d_binds.size());
            ImGui::TreePop();
        }
    }
    ImGui::End();
}
