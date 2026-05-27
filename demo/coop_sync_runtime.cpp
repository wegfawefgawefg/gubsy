#include "demo/coop_sync_runtime.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/net_transport.hpp"
#include "src/session_link.hpp"
#include "demo/coop_sync_wire.hpp"

namespace {

constexpr double kInputPushIntervalSec = 1.0 / 20.0;
constexpr double kSnapshotPushIntervalSec = 1.0 / 20.0;
constexpr double kInitialSnapshotTimeoutSec = 4.0;
constexpr double kSnapshotStreamTimeoutSec = 4.0;

struct CoopSyncRuntime {
    CoopSyncHooks hooks{};
    CoopSyncDriver driver{};
    bool configured{false};
    bool active{false};
    bool is_host{false};
    bool has_authoritative_snapshot{false};
    std::string server_url;
    std::string room_code;
    std::string host_secret;
    std::string local_member_id;
    SessionContract contract{};
    std::string status_text;
    std::string last_error;
    double connected_at{0.0};
    double last_packet_received_at{0.0};
    double last_snapshot_received_at{0.0};
    bool snapshot_timed_out{false};
    std::vector<std::string> member_ids;
    std::vector<std::vector<std::uint8_t>> current_inputs;
    std::vector<std::vector<std::uint8_t>> previous_inputs;
    std::vector<std::uint64_t> current_input_seqs;
    std::vector<std::uint64_t> previous_input_seqs;
    std::uint64_t sim_frame{0};
    std::uint64_t last_applied_snapshot_frame{0};
    std::uint64_t next_local_input_seq{1};
    std::uint64_t last_acked_local_input_seq{0};
    std::vector<std::uint8_t> last_acked_local_input;
    std::deque<CoopSequencedInput> pending_local_inputs;
    double next_input_push_at{0.0};
    double next_snapshot_push_at{0.0};
};

CoopSyncRuntime g_sync;

double runtime_now() {
    if (g_sync.hooks.link.query_now)
        return g_sync.hooks.link.query_now(g_sync.hooks.link.ctx);
    return 0.0;
}

int find_member_index(const CoopSyncRuntime& runtime, const std::string& member_id) {
    for (std::size_t i = 0; i < runtime.member_ids.size(); ++i) {
        if (runtime.member_ids[i] == member_id)
            return static_cast<int>(i);
    }
    return -1;
}

void rebuild_member_buffers(const std::vector<std::string>& member_ids) {
    std::unordered_map<std::string, std::vector<std::uint8_t>> current_by_member;
    std::unordered_map<std::string, std::vector<std::uint8_t>> previous_by_member;
    std::unordered_map<std::string, std::uint64_t> current_seq_by_member;
    std::unordered_map<std::string, std::uint64_t> previous_seq_by_member;
    for (std::size_t i = 0; i < g_sync.member_ids.size(); ++i) {
        current_by_member[g_sync.member_ids[i]] = g_sync.current_inputs[i];
        previous_by_member[g_sync.member_ids[i]] = g_sync.previous_inputs[i];
        current_seq_by_member[g_sync.member_ids[i]] = g_sync.current_input_seqs[i];
        previous_seq_by_member[g_sync.member_ids[i]] = g_sync.previous_input_seqs[i];
    }

    g_sync.member_ids = member_ids;
    g_sync.current_inputs.assign(member_ids.size(), {});
    g_sync.previous_inputs.assign(member_ids.size(), {});
    g_sync.current_input_seqs.assign(member_ids.size(), 0);
    g_sync.previous_input_seqs.assign(member_ids.size(), 0);
    for (std::size_t i = 0; i < member_ids.size(); ++i) {
        auto current_it = current_by_member.find(member_ids[i]);
        if (current_it != current_by_member.end())
            g_sync.current_inputs[i] = current_it->second;
        auto previous_it = previous_by_member.find(member_ids[i]);
        if (previous_it != previous_by_member.end())
            g_sync.previous_inputs[i] = previous_it->second;
        auto current_seq_it = current_seq_by_member.find(member_ids[i]);
        if (current_seq_it != current_seq_by_member.end())
            g_sync.current_input_seqs[i] = current_seq_it->second;
        auto previous_seq_it = previous_seq_by_member.find(member_ids[i]);
        if (previous_seq_it != previous_seq_by_member.end())
            g_sync.previous_input_seqs[i] = previous_seq_it->second;
    }
}

void refresh_member_ids() {
    if (!g_sync.hooks.query_member_ids)
        return;
    std::vector<std::string> member_ids;
    g_sync.hooks.query_member_ids(g_sync.hooks.link.ctx, member_ids);
    if (member_ids.empty() && !g_sync.local_member_id.empty())
        member_ids.push_back(g_sync.local_member_id);
    if (!g_sync.local_member_id.empty() &&
        std::find(member_ids.begin(), member_ids.end(), g_sync.local_member_id) == member_ids.end()) {
        member_ids.push_back(g_sync.local_member_id);
    }
    rebuild_member_buffers(member_ids);
}

void set_status_line() {
    if (!g_sync.active) {
        g_sync.status_text = "Offline";
        return;
    }
    g_sync.status_text = std::string(g_sync.is_host ? "Online Host" : "Online Client") +
                         " | room " + g_sync.room_code +
                         " | peers " + std::to_string(g_sync.member_ids.size());
    if (!g_sync.is_host) {
        if (g_sync.contract.realtime_endpoint.empty())
            g_sync.status_text += " | waiting for endpoint";
        else if (g_sync.snapshot_timed_out)
            g_sync.status_text += " | snapshot timeout";
        else if (!g_sync.has_authoritative_snapshot)
            g_sync.status_text += " | waiting for snapshot";
    }
}

void start_connection(const SessionLinkConnection& connection) {
    CoopSyncRuntime next = g_sync;
    next.active = true;
    next.is_host = connection.is_host;
    next.server_url = connection.server_url;
    next.room_code = coop_sync_normalized_room_code(connection.room_code);
    next.host_secret = connection.host_secret;
    next.local_member_id = connection.local_member_id;
    next.contract = connection.contract;
    next.has_authoritative_snapshot = false;
    next.connected_at = runtime_now();
    next.last_packet_received_at = next.connected_at;
    next.last_snapshot_received_at = next.connected_at;
    next.snapshot_timed_out = false;
    next.member_ids.clear();
    next.current_inputs.clear();
    next.previous_inputs.clear();
    next.current_input_seqs.clear();
    next.previous_input_seqs.clear();
    next.sim_frame = 0;
    next.last_applied_snapshot_frame = 0;
    next.next_local_input_seq = 1;
    next.last_acked_local_input_seq = 0;
    next.last_acked_local_input.clear();
    next.pending_local_inputs.clear();
    next.next_input_push_at = 0.0;
    next.next_snapshot_push_at = 0.0;
    g_sync = std::move(next);
    if (g_sync.driver.reset_runtime)
        g_sync.driver.reset_runtime(g_sync.driver.ctx);
    refresh_member_ids();
    set_status_line();
}

bool ensure_connection(std::string& err) {
    if (!g_sync.configured)
        return false;
    if (!session_link_update(err)) {
        coop_sync_reset();
        if (!err.empty())
            g_sync.last_error = err;
        return false;
    }

    SessionLinkConnection connection;
    if (!session_link_query_connection(connection) || !connection.active) {
        coop_sync_reset();
        return false;
    }
    const std::string normalized_code = coop_sync_normalized_room_code(connection.room_code);
    if (!g_sync.active ||
        g_sync.is_host != connection.is_host ||
        g_sync.local_member_id != connection.local_member_id ||
        !session_contract_equal(g_sync.contract, connection.contract) ||
        g_sync.room_code != normalized_code ||
        g_sync.server_url != connection.server_url) {
        start_connection(connection);
    } else if (g_sync.member_ids.empty()) {
        refresh_member_ids();
    }

    set_status_line();
    return g_sync.active;
}

bool collect_transport_inputs(std::string& err) {
    std::vector<NetTransportPacket> packets;
    if (!session_link_poll(packets, err))
        return false;
    if (!packets.empty())
        g_sync.last_packet_received_at = runtime_now();
    for (const NetTransportPacket& packet : packets) {
        if (packet.kind != NetPacketKind::Input)
            continue;
        int index = find_member_index(g_sync, packet.member_id);
        if (index < 0)
            continue;
        g_sync.current_inputs[static_cast<std::size_t>(index)] = packet.payload;
        g_sync.current_input_seqs[static_cast<std::size_t>(index)] = packet.seq;
    }
    set_status_line();
    return true;
}

bool publish_snapshot(std::string& err) {
    if (!g_sync.driver.capture_snapshot)
        return false;
    std::vector<std::uint8_t> snapshot;
    if (!g_sync.driver.capture_snapshot(g_sync.driver.ctx, g_sync.member_ids, g_sync.sim_frame, snapshot))
        return false;

    CoopSnapshotEnvelope snapshot_packet;
    snapshot_packet.sim_frame = g_sync.sim_frame;
    snapshot_packet.driver_snapshot = std::move(snapshot);
    snapshot_packet.acked_inputs.reserve(g_sync.member_ids.size());
    for (std::size_t i = 0; i < g_sync.member_ids.size(); ++i)
        snapshot_packet.acked_inputs.push_back({g_sync.member_ids[i], g_sync.current_input_seqs[i]});

    NetTransportPacket packet;
    packet.kind = NetPacketKind::Snapshot;
    packet.room_code = g_sync.room_code;
    if (!coop_sync_encode_snapshot_envelope(snapshot_packet, packet.payload, err))
        return false;
    return session_link_send(packet, err);
}

void prune_acked_local_inputs(std::uint64_t acked_local_seq) {
    while (!g_sync.pending_local_inputs.empty() &&
           g_sync.pending_local_inputs.front().seq <= acked_local_seq) {
        g_sync.last_acked_local_input = g_sync.pending_local_inputs.front().payload;
        g_sync.pending_local_inputs.pop_front();
    }
    g_sync.last_acked_local_input_seq = std::max(g_sync.last_acked_local_input_seq, acked_local_seq);
}

void replay_pending_local_inputs(float dt, const std::vector<std::uint8_t>& latest_local_input) {
    const int local_index = find_member_index(g_sync, g_sync.local_member_id);
    if (local_index < 0)
        return;

    if (g_sync.pending_local_inputs.empty()) {
        g_sync.current_inputs[static_cast<std::size_t>(local_index)] = latest_local_input;
        g_sync.current_input_seqs[static_cast<std::size_t>(local_index)] = g_sync.last_acked_local_input_seq;
        g_sync.previous_inputs[static_cast<std::size_t>(local_index)] = latest_local_input;
        g_sync.previous_input_seqs[static_cast<std::size_t>(local_index)] = g_sync.last_acked_local_input_seq;
        if (g_sync.driver.apply_local_view_input)
            g_sync.driver.apply_local_view_input(g_sync.driver.ctx, latest_local_input);
        return;
    }

    std::vector<std::vector<std::uint8_t>> replay_previous = g_sync.current_inputs;
    std::vector<std::vector<std::uint8_t>> replay_current = g_sync.current_inputs;
    replay_previous[static_cast<std::size_t>(local_index)] = g_sync.last_acked_local_input;
    replay_current[static_cast<std::size_t>(local_index)] = g_sync.last_acked_local_input;

    for (const CoopSequencedInput& pending : g_sync.pending_local_inputs) {
        replay_current[static_cast<std::size_t>(local_index)] = pending.payload;
        if (g_sync.driver.predict) {
            g_sync.driver.predict(g_sync.driver.ctx,
                                  g_sync.member_ids,
                                  replay_current,
                                  replay_previous,
                                  dt);
        }
        replay_previous = replay_current;
    }

    g_sync.current_inputs = replay_current;
    g_sync.previous_inputs = replay_previous;
    g_sync.current_input_seqs[static_cast<std::size_t>(local_index)] = g_sync.pending_local_inputs.back().seq;
    g_sync.previous_input_seqs[static_cast<std::size_t>(local_index)] =
        g_sync.current_input_seqs[static_cast<std::size_t>(local_index)];
    if (g_sync.driver.apply_local_view_input)
        g_sync.driver.apply_local_view_input(g_sync.driver.ctx, latest_local_input);
}

bool fetch_snapshot(const std::vector<std::uint8_t>& latest_local_input, float dt, std::string& err) {
    if (!g_sync.driver.apply_snapshot)
        return false;
    std::vector<NetTransportPacket> packets;
    if (!session_link_poll(packets, err))
        return false;
    if (!packets.empty())
        g_sync.last_packet_received_at = runtime_now();

    CoopSnapshotEnvelope envelope;
    bool has_snapshot = false;
    for (const NetTransportPacket& packet : packets) {
        if (packet.kind != NetPacketKind::Snapshot)
            continue;
        if (!coop_sync_decode_snapshot_envelope(packet.payload, envelope, err))
            return false;
        has_snapshot = true;
    }
    if (!has_snapshot)
        return true;
    if (envelope.sim_frame <= g_sync.last_applied_snapshot_frame)
        return true;

    std::uint64_t acked_local_seq = 0;
    for (const auto& ack : envelope.acked_inputs) {
        if (ack.first == g_sync.local_member_id) {
            acked_local_seq = ack.second;
            break;
        }
    }
    prune_acked_local_inputs(acked_local_seq);

    if (g_sync.driver.begin_reconcile)
        g_sync.driver.begin_reconcile(g_sync.driver.ctx);
    std::vector<std::string> member_ids;
    if (!g_sync.driver.apply_snapshot(g_sync.driver.ctx, envelope.driver_snapshot, member_ids))
        return false;
    rebuild_member_buffers(member_ids);
    g_sync.last_applied_snapshot_frame = envelope.sim_frame;
    g_sync.has_authoritative_snapshot = true;
    g_sync.last_snapshot_received_at = runtime_now();
    g_sync.snapshot_timed_out = false;
    g_sync.last_error.clear();
    replay_pending_local_inputs(dt, latest_local_input);
    if (g_sync.driver.finish_reconcile)
        g_sync.driver.finish_reconcile(g_sync.driver.ctx,
                                       g_sync.member_ids,
                                       g_sync.local_member_id,
                                       g_sync.is_host);
    return true;
}

void update_client_timeout_state() {
    if (!g_sync.active || g_sync.is_host)
        return;
    const double now = runtime_now();
    if (!g_sync.has_authoritative_snapshot) {
        if (now - g_sync.connected_at >= kInitialSnapshotTimeoutSec) {
            g_sync.snapshot_timed_out = true;
            g_sync.last_error = "Timed out waiting for host snapshot";
        }
        return;
    }
    if (now - g_sync.last_snapshot_received_at >= kSnapshotStreamTimeoutSec) {
        g_sync.snapshot_timed_out = true;
        g_sync.last_error = "Lost host snapshot stream";
    }
}

void run_host_step(const CoopSequencedInput& local_input, float dt, double now) {
    std::string transport_err;
    if (!collect_transport_inputs(transport_err) && !transport_err.empty())
        g_sync.last_error = transport_err;
    int local_index = find_member_index(g_sync, g_sync.local_member_id);
    if (local_index < 0) {
        std::vector<std::string> member_ids = g_sync.member_ids;
        member_ids.insert(member_ids.begin(), g_sync.local_member_id);
        rebuild_member_buffers(member_ids);
        local_index = 0;
    }

    g_sync.current_inputs[static_cast<std::size_t>(local_index)] = local_input.payload;
    g_sync.current_input_seqs[static_cast<std::size_t>(local_index)] = local_input.seq;
    if (g_sync.driver.predict) {
        g_sync.driver.predict(g_sync.driver.ctx,
                              g_sync.member_ids,
                              g_sync.current_inputs,
                              g_sync.previous_inputs,
                              dt);
    }

    if (now >= g_sync.next_snapshot_push_at) {
        if (publish_snapshot(transport_err))
            g_sync.next_snapshot_push_at = now + kSnapshotPushIntervalSec;
        else if (!transport_err.empty())
            g_sync.last_error = transport_err;
    }
    g_sync.previous_inputs = g_sync.current_inputs;
    g_sync.previous_input_seqs = g_sync.current_input_seqs;
    g_sync.sim_frame += 1;
}

void run_client_step(const CoopSequencedInput& local_input, float dt, double now) {
    int local_index = find_member_index(g_sync, g_sync.local_member_id);
    if (local_index < 0) {
        const std::string local_member_id = g_sync.local_member_id;
        std::vector<std::string> member_ids = g_sync.member_ids;
        member_ids.resize(member_ids.size() + 1);
        member_ids.back().assign(local_member_id);
        rebuild_member_buffers(member_ids);
        local_index = find_member_index(g_sync, local_member_id);
    }

    g_sync.current_inputs[static_cast<std::size_t>(local_index)] = local_input.payload;
    g_sync.current_input_seqs[static_cast<std::size_t>(local_index)] = local_input.seq;
    g_sync.pending_local_inputs.push_back(local_input);
    if (g_sync.has_authoritative_snapshot && g_sync.driver.predict) {
        g_sync.driver.predict(g_sync.driver.ctx,
                              g_sync.member_ids,
                              g_sync.current_inputs,
                              g_sync.previous_inputs,
                              dt);
    }
    if (g_sync.driver.apply_local_view_input)
        g_sync.driver.apply_local_view_input(g_sync.driver.ctx, local_input.payload);

    if (now >= g_sync.next_input_push_at) {
        std::string transport_err;
        NetTransportPacket packet;
        packet.kind = NetPacketKind::Input;
        packet.room_code = g_sync.room_code;
        packet.member_id = g_sync.local_member_id;
        packet.seq = local_input.seq;
        packet.payload = local_input.payload;
        if (session_link_send(packet, transport_err))
            g_sync.next_input_push_at = now + kInputPushIntervalSec;
        else if (!transport_err.empty())
            g_sync.last_error = transport_err;
    }
    std::string snapshot_err;
    if (!fetch_snapshot(local_input.payload, dt, snapshot_err) && !snapshot_err.empty())
        g_sync.last_error = snapshot_err;
    g_sync.previous_inputs = g_sync.current_inputs;
    g_sync.previous_input_seqs = g_sync.current_input_seqs;
}

} // namespace

void coop_sync_configure(const CoopSyncHooks& hooks, const CoopSyncDriver& driver) {
    g_sync.hooks = hooks;
    g_sync.driver = driver;
    g_sync.configured = true;
}

void coop_sync_reset() {
    g_sync.active = false;
    g_sync.is_host = false;
    g_sync.has_authoritative_snapshot = false;
    g_sync.server_url.clear();
    g_sync.room_code.clear();
    g_sync.host_secret.clear();
    g_sync.local_member_id.clear();
    g_sync.contract = SessionContract{};
    g_sync.contract.net_protocol = session_contract_default_net_protocol();
    g_sync.status_text = "Offline";
    g_sync.last_error.clear();
    g_sync.connected_at = 0.0;
    g_sync.last_packet_received_at = 0.0;
    g_sync.last_snapshot_received_at = 0.0;
    g_sync.snapshot_timed_out = false;
    g_sync.member_ids.clear();
    g_sync.current_inputs.clear();
    g_sync.previous_inputs.clear();
    g_sync.current_input_seqs.clear();
    g_sync.previous_input_seqs.clear();
    g_sync.sim_frame = 0;
    g_sync.last_applied_snapshot_frame = 0;
    g_sync.next_local_input_seq = 1;
    g_sync.last_acked_local_input_seq = 0;
    g_sync.last_acked_local_input.clear();
    g_sync.pending_local_inputs.clear();
    g_sync.next_input_push_at = 0.0;
    g_sync.next_snapshot_push_at = 0.0;
}

bool coop_sync_active() {
    return g_sync.active;
}

CoopSyncStepResult coop_sync_step(float dt) {
    CoopSyncStepResult result;
    std::string link_err;
    if (!ensure_connection(link_err))
        return result;

    refresh_member_ids();
    std::vector<std::uint8_t> local_input;
    if (!g_sync.driver.build_local_input || !g_sync.driver.build_local_input(g_sync.driver.ctx, local_input))
        return result;

    result.handled = true;
    const double now = runtime_now();

    CoopSequencedInput sequenced_local_input;
    sequenced_local_input.seq = g_sync.next_local_input_seq++;
    sequenced_local_input.payload = std::move(local_input);

    if (g_sync.is_host)
        run_host_step(sequenced_local_input, dt, now);
    else
        run_client_step(sequenced_local_input, dt, now);
    update_client_timeout_state();
    set_status_line();
    if (g_sync.driver.tick_correction) {
        g_sync.driver.tick_correction(g_sync.driver.ctx,
                                      g_sync.member_ids,
                                      g_sync.local_member_id,
                                      g_sync.is_host,
                                      dt);
    }
    return result;
}

const std::string& coop_sync_status_text() {
    return g_sync.status_text;
}

const std::string& coop_sync_last_error() {
    return g_sync.last_error;
}

const std::string& coop_sync_advertised_endpoint() {
    return session_link_advertised_endpoint();
}

bool coop_sync_query_stats(CoopSyncStats& out) {
    out = CoopSyncStats{};
    if (!g_sync.active)
        return false;

    const double now = runtime_now();
    out.active = g_sync.active;
    out.is_host = g_sync.is_host;
    out.has_authoritative_snapshot = g_sync.has_authoritative_snapshot;
    out.snapshot_timed_out = g_sync.snapshot_timed_out;
    out.member_count = g_sync.member_ids.size();
    out.pending_local_input_count = g_sync.pending_local_inputs.size();
    out.sim_frame = g_sync.sim_frame;
    out.last_applied_snapshot_frame = g_sync.last_applied_snapshot_frame;
    out.last_acked_local_input_seq = g_sync.last_acked_local_input_seq;
    out.connected_for_sec = std::max(0.0, now - g_sync.connected_at);
    out.packet_idle_sec = std::max(0.0, now - g_sync.last_packet_received_at);
    out.snapshot_idle_sec = std::max(0.0, now - g_sync.last_snapshot_received_at);
    return true;
}
