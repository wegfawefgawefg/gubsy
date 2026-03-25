#include "game/coop_session.hpp"

#include <algorithm>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/globals.hpp"
#include "engine/sync_session.hpp"
#include "game/coop_correction.hpp"
#include "game/coop_protocol.hpp"
#include "game/coop_sim.hpp"
#include "game/in_game_menu.hpp"
#include "game/input_frame.hpp"
#include "game/menu/lobby_online.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/settings.hpp"
#include "game/state.hpp"

namespace {

bool g_sync_configured = false;

bool query_connection(void*, SyncConnectionInfo& out) {
    const LobbySession& lobby = lobby_state_const();
    if (!lobby.online.in_room ||
        !session_contract_is_in_game(lobby.online.contract) ||
        lobby.online.compatibility != SessionCompatibility::Compatible)
        return false;
    out.active = true;
    out.is_host = lobby.online.is_host;
    out.server_url = lobby.online.server_url;
    out.room_code = lobby.online.room_code;
    out.host_secret = lobby.online.host_secret;
    out.local_member_id = lobby.online.member_id;
    out.contract = lobby.online.contract;
    return true;
}

void query_member_ids(void*, std::vector<std::string>& out) {
    const LobbySession& lobby = lobby_state_const();
    out.clear();
    out.reserve(lobby.online.members.size() + 1);
    for (const auto& member : lobby.online.members) {
        if (!member.member_id.empty())
            out.push_back(member.member_id);
    }
    if (!lobby.online.member_id.empty() &&
        std::find(out.begin(), out.end(), lobby.online.member_id) == out.end()) {
        out.push_back(lobby.online.member_id);
    }
}

void tick_presence(void*) {
    lobby_online_tick(lobby_state());
}

double query_now(void*) {
    return es ? es->now : 0.0;
}

bool build_local_input(void*, nlohmann::json& out) {
    if (!es)
        return false;
    InputFrame frame;
    if (!in_game_menu_blocks_game_input())
        build_input_frame(0, es->device_state, frame);
    out = input_frame_to_json(frame);
    return true;
}

void predict_demo_world(void*,
                        const std::vector<std::string>&,
                        const std::vector<nlohmann::json>& current_inputs_json,
                        const std::vector<nlohmann::json>& previous_inputs_json,
                        float dt) {
    if (!ss)
        return;

    std::vector<InputFrame> current_inputs(current_inputs_json.size());
    std::vector<InputFrame> previous_inputs(previous_inputs_json.size());
    for (std::size_t i = 0; i < current_inputs_json.size(); ++i)
        input_frame_from_json(current_inputs_json[i], current_inputs[i]);
    for (std::size_t i = 0; i < previous_inputs_json.size(); ++i)
        input_frame_from_json(previous_inputs_json[i], previous_inputs[i]);

    ensure_demo_player_count(*ss, current_inputs.size());
    simulate_demo_world(*ss, current_inputs, previous_inputs, dt);
}

bool capture_demo_snapshot(void*,
                           const std::vector<std::string>& member_ids,
                           std::uint64_t sim_frame,
                           nlohmann::json& out) {
    if (!ss)
        return false;
    out = coop_snapshot_to_json(capture_coop_snapshot(*ss, member_ids, sim_frame));
    return true;
}

bool apply_demo_snapshot(void*, const nlohmann::json& snapshot_json, std::vector<std::string>& member_ids_out) {
    if (!ss)
        return false;
    CoopStateSnapshot snapshot;
    if (!coop_snapshot_from_json(snapshot_json, snapshot))
        return false;
    apply_coop_snapshot(snapshot, *ss, member_ids_out);
    return true;
}

void apply_local_view_input(void*, const nlohmann::json& input_json) {
    if (!ss)
        return;
    InputFrame frame;
    if (!input_frame_from_json(input_json, frame))
        return;
    apply_demo_view_input(*ss, frame);
}

void begin_reconcile(void*) {
    if (!ss)
        return;
    std::vector<std::string> member_ids;
    query_member_ids(nullptr, member_ids);
    demo_sync_correction_begin(*ss, member_ids);
}

void finish_reconcile(void*,
                      const std::vector<std::string>& member_ids,
                      const std::string& local_member_id,
                      bool is_host) {
    if (!ss)
        return;
    demo_sync_correction_finish(*ss, member_ids, local_member_id, is_host);
}

void tick_correction(void*,
                     const std::vector<std::string>& member_ids,
                     const std::string& local_member_id,
                     bool is_host,
                     float dt) {
    if (!ss)
        return;
    demo_sync_correction_tick(*ss, member_ids, local_member_id, is_host, dt);
}

void reset_runtime(void*) {
    demo_sync_correction_reset();
}

void ensure_sync_configured() {
    if (g_sync_configured)
        return;
    SyncSessionHooks hooks;
    hooks.query_connection = query_connection;
    hooks.query_member_ids = query_member_ids;
    hooks.tick_presence = tick_presence;
    hooks.query_now = query_now;

    SyncDriver driver;
    driver.build_local_input = build_local_input;
    driver.predict = predict_demo_world;
    driver.capture_snapshot = capture_demo_snapshot;
    driver.apply_snapshot = apply_demo_snapshot;
    driver.apply_local_view_input = apply_local_view_input;
    driver.begin_reconcile = begin_reconcile;
    driver.finish_reconcile = finish_reconcile;
    driver.tick_correction = tick_correction;
    driver.reset_runtime = reset_runtime;

    sync_session_configure(hooks, driver);
    g_sync_configured = true;
}

} // namespace

void coop_session_reset() {
    ensure_sync_configured();
    sync_session_reset();
}

bool coop_session_active() {
    ensure_sync_configured();
    return sync_session_active();
}

CoopStepResult coop_session_step() {
    ensure_sync_configured();
    CoopStepResult result;
    if (!ss)
        return result;

    const std::uint64_t previous_bonk_serial = ss->bonk_serial;
    SyncStepResult sync_result = sync_session_step(FIXED_TIMESTEP);
    result.handled = sync_result.handled;
    if (sync_result.handled && ss->bonk_serial > previous_bonk_serial)
        result.bonk_count = static_cast<int>(ss->bonk_serial - previous_bonk_serial);
    return result;
}

const std::string& coop_session_status_text() {
    ensure_sync_configured();
    return sync_session_status_text();
}

const std::string& coop_session_last_error() {
    ensure_sync_configured();
    return sync_session_last_error();
}

bool coop_session_query_stats(SyncSessionStats& out) {
    ensure_sync_configured();
    return sync_session_query_stats(out);
}
