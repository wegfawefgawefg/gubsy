#include "engine/imgui_debug/imgui_debug.hpp"

#include "engine/imgui_debug/windows.hpp"

#include <imgui.h>

namespace {

bool g_debug_enabled = false;
bool g_bar_visible = true;
bool g_show_players = false;
bool g_show_binds = false;
bool g_show_layouts = false;
bool g_show_video = false;

struct WindowToggle {
    const char* label;
    bool* flag;
    ImGuiKey hotkey;
    const char* hotkey_label;
};

constexpr WindowToggle kWindowToggles[] = {
    {"Players", &g_show_players, ImGuiKey_F1, "F1"},
    {"Binds", &g_show_binds, ImGuiKey_F2, "F2"},
    {"UI Layouts", &g_show_layouts, ImGuiKey_F3, "F3"},
    {"Video/Resolution", &g_show_video, ImGuiKey_F4, "F4"},
};

bool any_window_visible() {
    for (const auto& toggle : kWindowToggles) {
        if (*toggle.flag)
            return true;
    }
    return false;
}

void render_debug_bar() {
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove;
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("DebugHUD", nullptr, flags)) {
        ImGui::TextUnformatted("Debug Overlays");
        ImGui::Separator();
        for (const auto& toggle : kWindowToggles) {
            ImGui::Checkbox(toggle.label, toggle.flag);
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", toggle.hotkey_label);
        }
        ImGui::Separator();
        if (ImGui::Button("Hide bar (F9)"))
            g_bar_visible = false;
        ImGui::SameLine();
        ImGui::TextDisabled("F10 toggles all debug");
    }
    ImGui::End();
}

} // namespace

void imgui_debug_begin_frame(float /*dt*/) {
    if (!ImGui::GetCurrentContext())
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_F10))
        g_debug_enabled = !g_debug_enabled;

    if (!g_debug_enabled)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_F9))
        g_bar_visible = !g_bar_visible;

    for (const auto& toggle : kWindowToggles) {
        if (ImGui::IsKeyPressed(toggle.hotkey))
            *toggle.flag = !*toggle.flag;
    }
}

void imgui_debug_render() {
    if (!g_debug_enabled)
        return;

    if (!g_bar_visible && !any_window_visible())
        return;

    if (g_bar_visible)
        render_debug_bar();

    imgui_debug_render_players_window(&g_show_players);
    imgui_debug_render_binds_window(&g_show_binds);
    imgui_debug_render_layout_window(&g_show_layouts);
    imgui_debug_render_video_window(&g_show_video);
}

void imgui_debug_shutdown() {
    g_debug_enabled = false;
    g_bar_visible = true;
    g_show_players = false;
    g_show_binds = false;
    g_show_layouts = false;
    g_show_video = false;
}
