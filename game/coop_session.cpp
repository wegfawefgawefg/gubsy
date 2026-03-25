#include "game/coop_session.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/globals.hpp"
#include "engine/session_link.hpp"
#include "engine/sync_payload_codec.hpp"
#include "game/coop_correction.hpp"
#include "game/coop_protocol.hpp"
#include "game/coop_sync_runtime.hpp"
#include "game/coop_sim.hpp"
#include "game/in_game_menu.hpp"
#include "game/input_frame.hpp"
#include "game/menu/lobby_online.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/settings.hpp"
#include "game/state.hpp"

namespace {

bool g_sync_configured = false;
State* g_state = nullptr;

bool query_connection(void*, SessionLinkConnection& out) {
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

bool encode_json_payload(const nlohmann::json& json, std::vector<std::uint8_t>& out) {
    std::string err;
    return sync_payload_encode_json(json, out, err);
}

bool decode_json_payload(const std::vector<std::uint8_t>& bytes, nlohmann::json& out) {
    std::string err;
    return sync_payload_decode_json(bytes, out, err);
}

bool build_local_input(void*, std::vector<std::uint8_t>& out) {
    if (!es)
        return false;
    InputFrame frame;
    if (!in_game_menu_blocks_game_input())
        build_input_frame(0, es->device_state, frame);
    return encode_json_payload(input_frame_to_json(frame), out);
}

void predict_demo_world(void* ctx,
                        const std::vector<std::string>&,
                        const std::vector<std::vector<std::uint8_t>>& current_inputs_bytes,
                        const std::vector<std::vector<std::uint8_t>>& previous_inputs_bytes,
                        float dt) {
    (void)ctx;
    State* state = g_state;
    if (!state)
        return;

    std::vector<InputFrame> current_inputs(current_inputs_bytes.size());
    std::vector<InputFrame> previous_inputs(previous_inputs_bytes.size());
    for (std::size_t i = 0; i < current_inputs_bytes.size(); ++i) {
        nlohmann::json json;
        if (decode_json_payload(current_inputs_bytes[i], json))
            input_frame_from_json(json, current_inputs[i]);
    }
    for (std::size_t i = 0; i < previous_inputs_bytes.size(); ++i) {
        nlohmann::json json;
        if (decode_json_payload(previous_inputs_bytes[i], json))
            input_frame_from_json(json, previous_inputs[i]);
    }

    ensure_demo_player_count(*state, current_inputs.size());
    simulate_demo_world(*state, current_inputs, previous_inputs, dt);
}

bool capture_demo_snapshot(void* ctx,
                           const std::vector<std::string>& member_ids,
                           std::uint64_t sim_frame,
                           std::vector<std::uint8_t>& out) {
    (void)ctx;
    State* state = g_state;
    if (!state)
        return false;
    return encode_json_payload(coop_snapshot_to_json(capture_coop_snapshot(*state, member_ids, sim_frame)), out);
}

bool apply_demo_snapshot(void* ctx,
                         const std::vector<std::uint8_t>& snapshot_bytes,
                         std::vector<std::string>& member_ids_out) {
    (void)ctx;
    State* state = g_state;
    if (!state)
        return false;
    nlohmann::json snapshot_json;
    if (!decode_json_payload(snapshot_bytes, snapshot_json))
        return false;
    CoopStateSnapshot snapshot;
    if (!coop_snapshot_from_json(snapshot_json, snapshot))
        return false;
    apply_coop_snapshot(snapshot, *state, member_ids_out);
    return true;
}

void apply_local_view_input(void* ctx, const std::vector<std::uint8_t>& input_bytes) {
    (void)ctx;
    State* state = g_state;
    if (!state)
        return;
    nlohmann::json input_json;
    if (!decode_json_payload(input_bytes, input_json))
        return;
    InputFrame frame;
    if (!input_frame_from_json(input_json, frame))
        return;
    apply_demo_view_input(*state, frame);
}

void begin_reconcile(void* ctx) {
    (void)ctx;
    State* state = g_state;
    if (!state)
        return;
    std::vector<std::string> member_ids;
    query_member_ids(nullptr, member_ids);
    demo_sync_correction_begin(*state, member_ids);
}

void finish_reconcile(void* ctx,
                      const std::vector<std::string>& member_ids,
                      const std::string& local_member_id,
                      bool is_host) {
    (void)ctx;
    State* state = g_state;
    if (!state)
        return;
    demo_sync_correction_finish(*state, member_ids, local_member_id, is_host);
}

void tick_correction(void* ctx,
                     const std::vector<std::string>& member_ids,
                     const std::string& local_member_id,
                     bool is_host,
                     float dt) {
    (void)ctx;
    State* state = g_state;
    if (!state)
        return;
    demo_sync_correction_tick(*state, member_ids, local_member_id, is_host, dt);
}

void reset_runtime(void*) {
    demo_sync_correction_reset();
}

void ensure_sync_configured(State* state = nullptr) {
    if (state)
        g_state = state;
    if (g_sync_configured)
        return;
    CoopSyncHooks hooks;
    hooks.link.query_connection = query_connection;
    hooks.link.tick_presence = tick_presence;
    hooks.link.query_now = query_now;
    hooks.query_member_ids = query_member_ids;

    session_link_configure(hooks.link);

    CoopSyncDriver driver;
    driver.build_local_input = build_local_input;
    driver.predict = predict_demo_world;
    driver.capture_snapshot = capture_demo_snapshot;
    driver.apply_snapshot = apply_demo_snapshot;
    driver.apply_local_view_input = apply_local_view_input;
    driver.begin_reconcile = begin_reconcile;
    driver.finish_reconcile = finish_reconcile;
    driver.tick_correction = tick_correction;
    driver.reset_runtime = reset_runtime;

    coop_sync_configure(hooks, driver);
    g_sync_configured = true;
}

} // namespace

void coop_session_reset() {
    ensure_sync_configured();
    coop_sync_reset();
}

bool coop_session_active() {
    ensure_sync_configured();
    return coop_sync_active();
}

CoopStepResult coop_session_step(State& state) {
    ensure_sync_configured(&state);
    CoopStepResult result;

    const std::uint64_t previous_bonk_serial = state.bonk_serial;
    CoopSyncStepResult sync_result = coop_sync_step(FIXED_TIMESTEP);
    result.handled = sync_result.handled;
    if (sync_result.handled && state.bonk_serial > previous_bonk_serial)
        result.bonk_count = static_cast<int>(state.bonk_serial - previous_bonk_serial);
    return result;
}

const std::string& coop_session_status_text() {
    ensure_sync_configured();
    return coop_sync_status_text();
}

const std::string& coop_session_last_error() {
    ensure_sync_configured();
    return coop_sync_last_error();
}

const std::string& coop_session_advertised_endpoint() {
    ensure_sync_configured();
    return coop_sync_advertised_endpoint();
}

bool coop_session_query_stats(CoopSyncStats& out) {
    ensure_sync_configured();
    return coop_sync_query_stats(out);
}
