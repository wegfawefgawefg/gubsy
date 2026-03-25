#include "engine/imgui_debug/imgui_debug.hpp"

#include "engine/imgui_debug/windows.hpp"

#include <imgui.h>

#include <vector>

namespace {

bool g_debug_enabled = false;
bool g_bar_visible = true;
bool g_show_binds = false;
bool g_show_layouts = false;
bool g_show_video = false;

struct EngineWindowToggle {
    const char* label;
    bool* flag;
    ImGuiKey hotkey;
    const char* hotkey_label;
    void (*render)(bool* open_flag);
};

struct AppWindowToggle {
    ImguiDebugWindowDef def{};
    bool open{false};
};

constexpr EngineWindowToggle kEngineWindowToggles[] = {
    {"Binds", &g_show_binds, ImGuiKey_F2, "F2", imgui_debug_render_binds_window},
    {"UI Layouts", &g_show_layouts, ImGuiKey_F3, "F3", imgui_debug_render_layout_window},
    {"Video/Resolution", &g_show_video, ImGuiKey_F4, "F4", imgui_debug_render_video_window},
};

std::vector<AppWindowToggle>& app_window_toggles() {
    static std::vector<AppWindowToggle> toggles;
    return toggles;
}

bool any_window_visible() {
    for (const auto& toggle : kEngineWindowToggles) {
        if (*toggle.flag)
            return true;
    }
    for (const auto& toggle : app_window_toggles()) {
        if (toggle.open)
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
        for (const auto& toggle : kEngineWindowToggles) {
            ImGui::Checkbox(toggle.label, toggle.flag);
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", toggle.hotkey_label);
        }
        for (auto& toggle : app_window_toggles()) {
            ImGui::Checkbox(toggle.def.label, &toggle.open);
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]",
                                toggle.def.hotkey_label ? toggle.def.hotkey_label : "-");
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

    for (const auto& toggle : kEngineWindowToggles) {
        if (ImGui::IsKeyPressed(toggle.hotkey))
            *toggle.flag = !*toggle.flag;
    }
    for (auto& toggle : app_window_toggles()) {
        if (toggle.def.hotkey != 0 &&
            ImGui::IsKeyPressed(static_cast<ImGuiKey>(toggle.def.hotkey))) {
            toggle.open = !toggle.open;
        }
    }
}

void imgui_debug_render() {
    if (!g_debug_enabled)
        return;

    if (!g_bar_visible && !any_window_visible())
        return;

    if (g_bar_visible)
        render_debug_bar();

    for (const auto& toggle : kEngineWindowToggles)
        toggle.render(toggle.flag);
    for (auto& toggle : app_window_toggles()) {
        if (toggle.def.render)
            toggle.def.render(&toggle.open);
    }
}

void imgui_debug_register_window(const ImguiDebugWindowDef& def) {
    if (!def.label || !def.render)
        return;
    app_window_toggles().push_back({def, false});
}

void imgui_debug_shutdown() {
    g_debug_enabled = false;
    g_bar_visible = true;
    g_show_binds = false;
    g_show_layouts = false;
    g_show_video = false;
    app_window_toggles().clear();
}
