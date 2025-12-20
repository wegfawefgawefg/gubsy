#include "engine/imgui_debug/windows.hpp"

#include "engine/globals.hpp"
#include "engine/player.hpp"
#include "engine/input_sources.hpp"
#include "engine/input_system.hpp"
#include "engine/binds_profiles.hpp"
#include "game/actions.hpp"

#include <imgui.h>

namespace {
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
}

void imgui_debug_render_players_window(bool* open_flag) {
    if (!open_flag || !*open_flag)
        return;
    if (!ImGui::Begin("Debug: Players & Devices", open_flag)) {
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
