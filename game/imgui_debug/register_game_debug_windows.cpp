#include "game/imgui_debug/register_game_debug_windows.hpp"

#include "engine/binds_profiles.hpp"
#include "engine/globals.hpp"
#include "engine/imgui_debug/imgui_debug.hpp"
#include "engine/input_sources.hpp"
#include "game/actions.hpp"
#include "game/coop_session.hpp"
#include "game/coop_sync_runtime.hpp"
#include "game/input_runtime.hpp"
#include "game/menu/lobby_state.hpp"
#include "engine/session_contract.hpp"

#include <imgui.h>

#include <string>

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

void text_or_dash(const char* label, const std::string& value) {
    ImGui::Text("%s: %s", label, value.empty() ? "-" : value.c_str());
}

void bool_text(const char* label, bool value) {
    ImGui::Text("%s: %s", label, value ? "yes" : "no");
}

void render_players_window(bool* open_flag) {
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
    if (es->players.empty())
        ImGui::TextUnformatted("No players registered.");
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

void render_required_mods(const SessionContract& contract) {
    if (contract.required_mod_ids.empty()) {
        ImGui::TextUnformatted("Required mods: (none)");
        return;
    }
    if (!ImGui::TreeNode("Required mods"))
        return;
    for (const std::string& mod_id : contract.required_mod_ids)
        ImGui::BulletText("%s", mod_id.c_str());
    ImGui::TreePop();
}

void render_members(const LobbyOnlineState& online) {
    if (online.members.empty()) {
        ImGui::TextUnformatted("Members: (none)");
        return;
    }
    if (!ImGui::TreeNode("Members"))
        return;
    for (const auto& member : online.members) {
        const char* host_tag = member.is_host ? " [host]" : "";
        const char* local_tag = member.is_local ? " [local]" : "";
        ImGui::BulletText("%s%s%s",
                          member.display_name.empty() ? member.member_id.c_str()
                                                      : member.display_name.c_str(),
                          host_tag,
                          local_tag);
        if (!member.member_id.empty())
            ImGui::TextDisabled("  id: %s", member.member_id.c_str());
    }
    ImGui::TreePop();
}

void render_realtime_stats() {
    CoopSyncStats stats;
    if (!coop_session_query_stats(stats)) {
        ImGui::TextUnformatted("Runtime stats: offline");
        return;
    }

    ImGui::Text("Member count: %zu", stats.member_count);
    ImGui::Text("Pending local inputs: %zu", stats.pending_local_input_count);
    ImGui::Text("Connected for: %.2fs", stats.connected_for_sec);
    ImGui::Text("Packet idle: %.2fs", stats.packet_idle_sec);
    ImGui::Text("Snapshot idle: %.2fs", stats.snapshot_idle_sec);
    ImGui::Text("Sim frame: %llu", static_cast<unsigned long long>(stats.sim_frame));
    ImGui::Text("Last snapshot frame: %llu",
                static_cast<unsigned long long>(stats.last_applied_snapshot_frame));
    ImGui::Text("Last acked local input: %llu",
                static_cast<unsigned long long>(stats.last_acked_local_input_seq));
    bool_text("Runtime host", stats.is_host);
    bool_text("Has authoritative snapshot", stats.has_authoritative_snapshot);
    bool_text("Snapshot timed out", stats.snapshot_timed_out);
}

void render_session_window(bool* open_flag) {
    if (!open_flag || !*open_flag)
        return;
    if (!ImGui::Begin("Debug: Session", open_flag)) {
        ImGui::End();
        return;
    }

    const LobbySession& lobby = lobby_state_const();
    const LobbyOnlineState& online = lobby.online;

    ImGui::TextUnformatted("Online Session");
    ImGui::Separator();
    bool_text("In room", online.in_room);
    bool_text("Host", online.is_host);
    bool_text("Reconnecting", online.reconnecting);
    bool_text("Session closed", online.session_closed);
    text_or_dash("Mode", online.contract.session_phase);
    text_or_dash("Room code", online.room_code);
    text_or_dash("Server URL", online.server_url);
    text_or_dash("Member ID", online.member_id);
    ImGui::Text("Members visible: %zu", online.members.size());
    ImGui::Text("Room failures: %d", online.room_failure_count);

    ImGui::Separator();
    ImGui::TextUnformatted("Status");
    text_or_dash("Lobby status", online.status_text);
    text_or_dash("Lobby error", online.last_error);
    text_or_dash("Close reason", online.session_close_reason);
    text_or_dash("Content status", online.content_status_text);
    ImGui::Text("Compatibility: %s",
                session_contract_compatibility_text(online.compatibility));

    ImGui::Separator();
    ImGui::TextUnformatted("Contract");
    text_or_dash("Game version", online.contract.game_version);
    text_or_dash("Net protocol", online.contract.net_protocol);
    text_or_dash("Realtime endpoint", online.contract.realtime_endpoint);
    text_or_dash("Mod hash", online.contract.mod_hash);
    ImGui::Text("Content revision: %llu",
                static_cast<unsigned long long>(online.contract.content_revision));
    ImGui::Text("Synced revision: %llu",
                static_cast<unsigned long long>(online.synced_content_revision));
    ImGui::Text("Failed revision: %llu",
                static_cast<unsigned long long>(online.failed_content_revision));
    bool_text("Allow live mod reload", online.contract.allow_live_mod_reload);
    render_required_mods(online.contract);

    ImGui::Separator();
    ImGui::TextUnformatted("Realtime Sync");
    bool_text("Coop sync active", coop_session_active());
    text_or_dash("Sync status", coop_session_status_text());
    text_or_dash("Sync error", coop_session_last_error());
    render_realtime_stats();

    ImGui::Separator();
    render_members(online);
    ImGui::Text("Rooms discovered: %zu", online.discovered_rooms.size());

    ImGui::End();
}

} // namespace

void register_game_debug_windows() {
    imgui_debug_register_window({"Players", ImGuiKey_F1, "F1", render_players_window});
    imgui_debug_register_window({"Session", ImGuiKey_F5, "F5", render_session_window});
}
