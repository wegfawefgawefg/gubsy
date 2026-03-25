#include "engine/imgui_debug/windows.hpp"

#include "engine/session_contract.hpp"
#include "engine/sync_session.hpp"
#include "game/coop_session.hpp"
#include "game/menu/lobby_state.hpp"

#include <imgui.h>

namespace {

void text_or_dash(const char* label, const std::string& value) {
    ImGui::Text("%s: %s", label, value.empty() ? "-" : value.c_str());
}

void bool_text(const char* label, bool value) {
    ImGui::Text("%s: %s", label, value ? "yes" : "no");
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
    SyncSessionStats stats;
    if (!coop_session_query_stats(stats)) {
        ImGui::TextUnformatted("Runtime stats: offline");
        return;
    }

    ImGui::Text("Member count: %zu", stats.member_count);
    ImGui::Text("Pending local inputs: %zu", stats.pending_local_input_count);
    ImGui::Text("Connected for: %.2fs", stats.connected_for_sec);
    ImGui::Text("Packet idle: %.2fs", stats.packet_idle_sec);
    ImGui::Text("Snapshot idle: %.2fs", stats.snapshot_idle_sec);
    ImGui::Text("Sim frame: %llu",
                static_cast<unsigned long long>(stats.sim_frame));
    ImGui::Text("Last snapshot frame: %llu",
                static_cast<unsigned long long>(stats.last_applied_snapshot_frame));
    ImGui::Text("Last acked local input: %llu",
                static_cast<unsigned long long>(stats.last_acked_local_input_seq));
    bool_text("Runtime host", stats.is_host);
    bool_text("Has authoritative snapshot", stats.has_authoritative_snapshot);
    bool_text("Snapshot timed out", stats.snapshot_timed_out);
}

} // namespace

void imgui_debug_render_session_window(bool* open_flag) {
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
