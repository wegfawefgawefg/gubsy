#include "engine/imgui_debug/imgui_debug.hpp"

#include "engine/globals.hpp"
#include "engine/player.hpp"
#include "engine/input_system.hpp"
#include "engine/input_sources.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/ui_layouts.hpp"
#include "game/actions.hpp"

#include <imgui.h>

namespace {

bool g_debug_enabled = true;
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

struct ActionLabel {
    int id;
    const char* label;
};

constexpr ActionLabel kTrackedActions[] = {
    {GameAction::MENU_UP, "MENU_UP"},
    {GameAction::MENU_DOWN, "MENU_DOWN"},
    {GameAction::MENU_LEFT, "MENU_LEFT"},
    {GameAction::MENU_RIGHT, "MENU_RIGHT"},
    {GameAction::MENU_SELECT, "MENU_SELECT"},
    {GameAction::MENU_BACK, "MENU_BACK"},
    {GameAction::UP, "UP"},
    {GameAction::DOWN, "DOWN"},
    {GameAction::LEFT, "LEFT"},
    {GameAction::RIGHT, "RIGHT"},
    {GameAction::USE, "USE"},
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

void render_players_window() {
    if (!g_show_players)
        return;
    if (!ImGui::Begin("Debug: Players & Devices", &g_show_players)) {
        ImGui::End();
        return;
    }
    if (!es) {
        ImGui::TextUnformatted("Engine state unavailable.");
        ImGui::End();
        return;
    }
    if (es->players.empty()) {
        ImGui::TextUnformatted("No players registered.");
    }
    for (std::size_t i = 0; i < es->players.size(); ++i) {
        const Player& player = es->players[i];
        ImGui::Separator();
        ImGui::Text("Player %zu", i);
        if (!player.has_active_profile) {
            ImGui::TextUnformatted("  No active user profile.");
            continue;
        }
        const UserProfile& profile = player.profile;
        ImGui::Text("  User Profile #%d%s", profile.id, profile.guest ? " (guest)" : "");
        ImGui::Text("  Name: %s", profile.name.c_str());
        ImGui::Text("  Binds Profile ID: %d", profile.last_binds_profile_id);
        ImGui::Text("  Input Settings Profile ID: %d", profile.last_input_settings_profile_id);
        ImGui::Text("  Game Settings Profile ID: %d", profile.last_game_settings_profile_id);

        const InputFrame& frame = current_input_frame(static_cast<int>(i));
        ImGui::Text("  Down Bits: 0x%08X", frame.down_bits);
        ImGui::TextUnformatted("  Active Actions:");
        ImGui::Indent();
        bool printed = false;
        for (const auto& action : kTrackedActions) {
            if (frame.down_bits & (1u << action.id)) {
                ImGui::BulletText("%s", action.label);
                printed = true;
            }
        }
        if (!printed)
            ImGui::TextDisabled("(none)");
        ImGui::Unindent();
    }
    if (!es->input_sources.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted("Detected Input Sources:");
        ImGui::Indent();
        for (const auto& source : es->input_sources) {
            const char* type = "Unknown";
            switch (source.type) {
                case InputSourceType::Keyboard: type = "Keyboard"; break;
                case InputSourceType::Mouse: type = "Mouse"; break;
                case InputSourceType::Gamepad: type = "Gamepad"; break;
            }
            ImGui::BulletText("%s (ID %d)", type, source.device_id.id);
        }
        ImGui::Unindent();
    }
    ImGui::End();
}

void render_binds_window() {
    if (!g_show_binds)
        return;
    if (!ImGui::Begin("Debug: Binds Profiles", &g_show_binds)) {
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

void render_layout_window() {
    if (!g_show_layouts)
        return;
    if (!ImGui::Begin("Debug: UI Layouts", &g_show_layouts)) {
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
    struct GroupKey { int id; std::string label; };
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
            group_open = ImGui::TreeNode(
                (layout->label + "_" + std::to_string(layout->id)).c_str(),
                "%s (Layout ID %d)", layout->label.c_str(), layout->id);
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

void render_video_window() {
    if (!g_show_video)
        return;
    if (!ImGui::Begin("Debug: Video & Resolution", &g_show_video)) {
        ImGui::End();
        return;
    }
    if (!gg || !gg->renderer) {
        ImGui::TextUnformatted("Graphics subsystem unavailable.");
        ImGui::End();
        return;
    }
    glm::ivec2 window_dims = get_window_dimensions();
    glm::ivec2 render_dims = get_render_dimensions();
    ImGui::Text("Window: %d x %d", window_dims.x, window_dims.y);
    ImGui::Text("Render: %d x %d", render_dims.x, render_dims.y);

    static int pending_window_w = 1280;
    static int pending_window_h = 720;
    static int pending_render_w = 1280;
    static int pending_render_h = 720;
    static bool initialized = false;
    if (!initialized) {
        pending_window_w = window_dims.x;
        pending_window_h = window_dims.y;
        pending_render_w = render_dims.x;
        pending_render_h = render_dims.y;
        initialized = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Render Resolution");
    ImGui::InputInt("Render Width", &pending_render_w);
    ImGui::InputInt("Render Height", &pending_render_h);
    if (ImGui::Button("Apply Render Resolution")) {
        if (set_render_resolution(pending_render_w, pending_render_h)) {
            glm::ivec2 updated = get_render_dimensions();
            pending_render_w = updated.x;
            pending_render_h = updated.y;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Match Window Size##render")) {
        pending_render_w = window_dims.x;
        pending_render_h = window_dims.y;
        set_render_resolution(pending_render_w, pending_render_h);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Render##render")) {
        pending_render_w = 1280;
        pending_render_h = 720;
        set_render_resolution(pending_render_w, pending_render_h);
    }
    if (ImGui::Button("1080p Render")) {
        pending_render_w = 1920;
        pending_render_h = 1080;
        set_render_resolution(pending_render_w, pending_render_h);
    }
    ImGui::SameLine();
    if (ImGui::Button("1440p Render")) {
        pending_render_w = 2560;
        pending_render_h = 1440;
        set_render_resolution(pending_render_w, pending_render_h);
    }
    ImGui::SameLine();
    if (ImGui::Button("2560x1080 Render")) {
        pending_render_w = 2560;
        pending_render_h = 1080;
        set_render_resolution(pending_render_w, pending_render_h);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Window");
    ImGui::InputInt("Window Width", &pending_window_w);
    ImGui::InputInt("Window Height", &pending_window_h);
    if (ImGui::Button("Apply Window Size")) {
        set_window_dimensions(pending_window_w, pending_window_h);
    }
    ImGui::SameLine();
    if (ImGui::Button("Match Render Size##window")) {
        pending_window_w = render_dims.x;
        pending_window_h = render_dims.y;
        set_window_dimensions(pending_window_w, pending_window_h);
    }

    const char* window_mode_labels[] = {"Windowed", "Borderless", "Fullscreen"};
    int window_mode = static_cast<int>(gg->window_mode);
    if (ImGui::Combo("Window Mode", &window_mode,
                     window_mode_labels, IM_ARRAYSIZE(window_mode_labels))) {
        set_window_display_mode(static_cast<WindowDisplayMode>(window_mode));
    }

    const char* scale_labels[] = {"Fit (letterbox)", "Stretch"};
    int scale_mode = static_cast<int>(gg->render_scale_mode);
    if (ImGui::Combo("Render Scale", &scale_mode, scale_labels, IM_ARRAYSIZE(scale_labels))) {
        set_render_scale_mode(static_cast<RenderScaleMode>(scale_mode));
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

    if (g_bar_visible || any_window_visible()) {
        if (g_bar_visible)
            render_debug_bar();
        render_players_window();
        render_binds_window();
        render_layout_window();
        render_video_window();
    }
}

void imgui_debug_shutdown() {
    g_debug_enabled = true;
    g_bar_visible = true;
    g_show_players = false;
    g_show_binds = false;
    g_show_layouts = false;
    g_show_video = false;
}
