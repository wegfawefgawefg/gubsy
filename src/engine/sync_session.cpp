#include "engine/sync_session.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/sync_session_wire.hpp"

namespace {

constexpr double kInputPushIntervalSec = 1.0 / 20.0;
constexpr double kInputPollIntervalSec = 1.0 / 20.0;
constexpr double kSnapshotPushIntervalSec = 1.0 / 20.0;
constexpr double kSnapshotPollIntervalSec = 1.0 / 20.0;

struct SyncRuntime {
    SyncSessionHooks hooks{};
    SyncDriver driver{};
    bool configured{false};
    bool active{false};
    bool is_host{false};
    bool has_authoritative_snapshot{false};
    std::string server_url;
    std::string room_code;
    std::string host_secret;
    std::string local_member_id;
    std::string status_text;
    std::string last_error;
    std::vector<std::string> member_ids;
    std::vector<nlohmann::json> current_inputs;
    std::vector<nlohmann::json> previous_inputs;
    std::vector<std::uint64_t> current_input_seqs;
    std::vector<std::uint64_t> previous_input_seqs;
    std::uint64_t sim_frame{0};
    std::uint64_t last_applied_snapshot_frame{0};
    std::uint64_t next_local_input_seq{1};
    std::uint64_t last_acked_local_input_seq{0};
    nlohmann::json last_acked_local_input = nlohmann::json::object();
    std::deque<SequencedInput> pending_local_inputs;
    double next_input_push_at{0.0};
    double next_input_poll_at{0.0};
    double next_snapshot_push_at{0.0};
    double next_snapshot_poll_at{0.0};
};

SyncRuntime g_sync;

int find_member_index(const SyncRuntime& runtime, const std::string& member_id) {
    for (std::size_t i = 0; i < runtime.member_ids.size(); ++i) {
        if (runtime.member_ids[i] == member_id)
            return static_cast<int>(i);
    }
    return -1;
}

void rebuild_member_buffers(const std::vector<std::string>& member_ids) {
    std::unordered_map<std::string, nlohmann::json> current_by_member;
    std::unordered_map<std::string, nlohmann::json> previous_by_member;
    std::unordered_map<std::string, std::uint64_t> current_seq_by_member;
    std::unordered_map<std::string, std::uint64_t> previous_seq_by_member;
    for (std::size_t i = 0; i < g_sync.member_ids.size(); ++i) {
        current_by_member[g_sync.member_ids[i]] = g_sync.current_inputs[i];
        previous_by_member[g_sync.member_ids[i]] = g_sync.previous_inputs[i];
        current_seq_by_member[g_sync.member_ids[i]] = g_sync.current_input_seqs[i];
        previous_seq_by_member[g_sync.member_ids[i]] = g_sync.previous_input_seqs[i];
    }

    g_sync.member_ids = member_ids;
    g_sync.current_inputs.assign(member_ids.size(), nlohmann::json::object());
    g_sync.previous_inputs.assign(member_ids.size(), nlohmann::json::object());
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
    g_sync.hooks.query_member_ids(g_sync.hooks.ctx, member_ids);
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
}

void start_connection(const SyncConnectionInfo& connection) {
    SyncRuntime next = g_sync;
    next.active = true;
    next.is_host = connection.is_host;
    next.server_url = connection.server_url;
    next.room_code = sync_session_normalized_room_code(connection.room_code);
    next.host_secret = connection.host_secret;
    next.local_member_id = connection.local_member_id;
    next.has_authoritative_snapshot = false;
    next.member_ids.clear();
    next.current_inputs.clear();
    next.previous_inputs.clear();
    next.current_input_seqs.clear();
    next.previous_input_seqs.clear();
    next.sim_frame = 0;
    next.last_applied_snapshot_frame = 0;
    next.next_local_input_seq = 1;
    next.last_acked_local_input_seq = 0;
    next.last_acked_local_input = nlohmann::json::object();
    next.pending_local_inputs.clear();
    next.next_input_push_at = 0.0;
    next.next_input_poll_at = 0.0;
    next.next_snapshot_push_at = 0.0;
    next.next_snapshot_poll_at = 0.0;
    g_sync = std::move(next);
    if (g_sync.driver.reset_runtime)
        g_sync.driver.reset_runtime(g_sync.driver.ctx);
    refresh_member_ids();
    set_status_line();
}

bool ensure_connection() {
    if (!g_sync.configured || !g_sync.hooks.query_connection)
        return false;
    SyncConnectionInfo connection;
    if (!g_sync.hooks.query_connection(g_sync.hooks.ctx, connection) || !connection.active) {
        sync_session_reset();
        return false;
    }
    const std::string normalized_code = sync_session_normalized_room_code(connection.room_code);
    if (!g_sync.active ||
        g_sync.is_host != connection.is_host ||
        g_sync.local_member_id != connection.local_member_id ||
        g_sync.room_code != normalized_code ||
        g_sync.server_url != connection.server_url) {
        start_connection(connection);
    } else if (g_sync.member_ids.empty()) {
        refresh_member_ids();
    }

    set_status_line();
    return g_sync.active;
}

bool publish_local_input(const SequencedInput& input, std::string& err) {
    nlohmann::json body;
    body["member_id"] = g_sync.local_member_id;
    body["input"] = sync_session_make_input_envelope(input);
    auto json = sync_session_post_json(g_sync.server_url,
                                       "/rooms/" + g_sync.room_code + "/input",
                                       body,
                                       err);
    return json.has_value();
}

bool fetch_room_inputs(std::string& err) {
    auto json = sync_session_get_json(g_sync.server_url,
                                      "/rooms/" + g_sync.room_code + "/inputs",
                                      err);
    if (!json)
        return false;
    auto members_it = json->find("members");
    if (members_it == json->end() || !members_it->is_array())
        return false;

    std::vector<std::string> member_ids;
    member_ids.reserve(members_it->size());
    for (const auto& member_json : *members_it) {
        const std::string member_id = member_json.value("member_id", "");
        if (!member_id.empty())
            member_ids.push_back(member_id);
    }
    rebuild_member_buffers(member_ids);

    for (const auto& member_json : *members_it) {
        const std::string member_id = member_json.value("member_id", "");
        int index = find_member_index(g_sync, member_id);
        if (index < 0)
            continue;
        auto input_it = member_json.find("input");
        if (input_it != member_json.end() && input_it->is_object()) {
            const SequencedInput input = sync_session_parse_input_envelope(*input_it);
            g_sync.current_inputs[static_cast<std::size_t>(index)] = input.payload;
            g_sync.current_input_seqs[static_cast<std::size_t>(index)] = input.seq;
        }
    }

    set_status_line();
    return true;
}

bool publish_snapshot(std::string& err) {
    if (!g_sync.driver.capture_snapshot)
        return false;
    nlohmann::json snapshot;
    if (!g_sync.driver.capture_snapshot(g_sync.driver.ctx, g_sync.member_ids, g_sync.sim_frame, snapshot))
        return false;

    nlohmann::json acked_inputs = nlohmann::json::object();
    for (std::size_t i = 0; i < g_sync.member_ids.size(); ++i)
        acked_inputs[g_sync.member_ids[i]] = g_sync.current_input_seqs[i];

    nlohmann::json body;
    body["member_id"] = g_sync.local_member_id;
    body["host_secret"] = g_sync.host_secret;
    body["snapshot"] = {
        {"sim_frame", g_sync.sim_frame},
        {"driver_snapshot", std::move(snapshot)},
        {"acked_inputs", std::move(acked_inputs)},
    };
    auto json = sync_session_post_json(g_sync.server_url,
                                       "/rooms/" + g_sync.room_code + "/snapshot",
                                       body,
                                       err);
    return json.has_value();
}

void prune_acked_local_inputs(std::uint64_t acked_local_seq) {
    while (!g_sync.pending_local_inputs.empty() &&
           g_sync.pending_local_inputs.front().seq <= acked_local_seq) {
        g_sync.last_acked_local_input = g_sync.pending_local_inputs.front().payload;
        g_sync.pending_local_inputs.pop_front();
    }
    g_sync.last_acked_local_input_seq = std::max(g_sync.last_acked_local_input_seq, acked_local_seq);
}

void replay_pending_local_inputs(float dt, const nlohmann::json& latest_local_input) {
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

    std::vector<nlohmann::json> replay_previous = g_sync.current_inputs;
    std::vector<nlohmann::json> replay_current = g_sync.current_inputs;
    if (!g_sync.last_acked_local_input.is_object())
        g_sync.last_acked_local_input = nlohmann::json::object();
    replay_previous[static_cast<std::size_t>(local_index)] = g_sync.last_acked_local_input;
    replay_current[static_cast<std::size_t>(local_index)] = g_sync.last_acked_local_input;

    for (const SequencedInput& pending : g_sync.pending_local_inputs) {
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
    g_sync.previous_input_seqs[static_cast<std::size_t>(local_index)] = g_sync.current_input_seqs[static_cast<std::size_t>(local_index)];
    if (g_sync.driver.apply_local_view_input)
        g_sync.driver.apply_local_view_input(g_sync.driver.ctx, latest_local_input);
}

bool fetch_snapshot(const nlohmann::json& latest_local_input, float dt, std::string& err) {
    if (!g_sync.driver.apply_snapshot)
        return false;
    auto json = sync_session_get_json(g_sync.server_url,
                                      "/rooms/" + g_sync.room_code + "/snapshot",
                                      err);
    if (!json)
        return false;
    if (!json->value("has_snapshot", false))
        return true;
    const nlohmann::json& snapshot = (*json)["snapshot"];
    const bool wrapped = snapshot.is_object() && snapshot.contains("driver_snapshot");
    const std::uint64_t sim_frame = snapshot.value("sim_frame", std::uint64_t{0});
    if (sim_frame <= g_sync.last_applied_snapshot_frame)
        return true;

    std::uint64_t acked_local_seq = 0;
    if (wrapped) {
        auto acked_it = snapshot.find("acked_inputs");
        if (acked_it != snapshot.end() && acked_it->is_object()) {
            auto local_ack_it = acked_it->find(g_sync.local_member_id);
            if (local_ack_it != acked_it->end() && local_ack_it->is_number_unsigned())
                acked_local_seq = local_ack_it->get<std::uint64_t>();
        }
    }
    prune_acked_local_inputs(acked_local_seq);

    if (g_sync.driver.begin_reconcile)
        g_sync.driver.begin_reconcile(g_sync.driver.ctx);
    std::vector<std::string> member_ids;
    const nlohmann::json& driver_snapshot = wrapped ? snapshot["driver_snapshot"] : snapshot;
    if (!g_sync.driver.apply_snapshot(g_sync.driver.ctx, driver_snapshot, member_ids))
        return false;
    rebuild_member_buffers(member_ids);
    g_sync.last_applied_snapshot_frame = sim_frame;
    g_sync.has_authoritative_snapshot = true;
    replay_pending_local_inputs(dt, latest_local_input);
    if (g_sync.driver.finish_reconcile)
        g_sync.driver.finish_reconcile(g_sync.driver.ctx,
                                       g_sync.member_ids,
                                       g_sync.local_member_id,
                                       g_sync.is_host);
    return true;
}

void run_host_step(const SequencedInput& local_input, float dt, double now) {
    if (now >= g_sync.next_input_poll_at) {
        std::string err;
        if (fetch_room_inputs(err))
            g_sync.next_input_poll_at = now + kInputPollIntervalSec;
        else if (!err.empty())
            g_sync.last_error = err;
    }
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
        std::string err;
        if (publish_snapshot(err))
            g_sync.next_snapshot_push_at = now + kSnapshotPushIntervalSec;
        else if (!err.empty())
            g_sync.last_error = err;
    }
    g_sync.previous_inputs = g_sync.current_inputs;
    g_sync.previous_input_seqs = g_sync.current_input_seqs;
    g_sync.sim_frame += 1;
}

void run_client_step(const SequencedInput& local_input, float dt, double now) {
    int local_index = find_member_index(g_sync, g_sync.local_member_id);
    if (local_index < 0) {
        std::vector<std::string> member_ids = g_sync.member_ids;
        member_ids.push_back(g_sync.local_member_id);
        rebuild_member_buffers(member_ids);
        local_index = static_cast<int>(g_sync.member_ids.size()) - 1;
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
        std::string err;
        if (publish_local_input(local_input, err))
            g_sync.next_input_push_at = now + kInputPushIntervalSec;
        else if (!err.empty())
            g_sync.last_error = err;
    }

    if (now >= g_sync.next_snapshot_poll_at) {
        std::string err;
        if (fetch_snapshot(local_input.payload, dt, err))
            g_sync.next_snapshot_poll_at = now + kSnapshotPollIntervalSec;
        else if (!err.empty())
            g_sync.last_error = err;
    }
    g_sync.previous_inputs = g_sync.current_inputs;
    g_sync.previous_input_seqs = g_sync.current_input_seqs;
}

} // namespace

void sync_session_configure(const SyncSessionHooks& hooks, const SyncDriver& driver) {
    g_sync.hooks = hooks;
    g_sync.driver = driver;
    g_sync.configured = true;
}

void sync_session_reset() {
    g_sync.active = false;
    g_sync.is_host = false;
    g_sync.has_authoritative_snapshot = false;
    g_sync.server_url.clear();
    g_sync.room_code.clear();
    g_sync.host_secret.clear();
    g_sync.local_member_id.clear();
    g_sync.status_text = "Offline";
    g_sync.last_error.clear();
    g_sync.member_ids.clear();
    g_sync.current_inputs.clear();
    g_sync.previous_inputs.clear();
    g_sync.current_input_seqs.clear();
    g_sync.previous_input_seqs.clear();
    g_sync.sim_frame = 0;
    g_sync.last_applied_snapshot_frame = 0;
    g_sync.next_local_input_seq = 1;
    g_sync.last_acked_local_input_seq = 0;
    g_sync.last_acked_local_input = nlohmann::json::object();
    g_sync.pending_local_inputs.clear();
    g_sync.next_input_push_at = 0.0;
    g_sync.next_input_poll_at = 0.0;
    g_sync.next_snapshot_push_at = 0.0;
    g_sync.next_snapshot_poll_at = 0.0;
}

bool sync_session_active() {
    return g_sync.active;
}

SyncStepResult sync_session_step(float dt) {
    SyncStepResult result;
    if (!ensure_connection())
        return result;

    if (g_sync.hooks.tick_presence)
        g_sync.hooks.tick_presence(g_sync.hooks.ctx);
    refresh_member_ids();

    nlohmann::json local_input;
    if (!g_sync.driver.build_local_input || !g_sync.driver.build_local_input(g_sync.driver.ctx, local_input))
        return result;

    result.handled = true;
    double now = 0.0;
    if (g_sync.hooks.query_now)
        now = g_sync.hooks.query_now(g_sync.hooks.ctx);

    SequencedInput sequenced_local_input;
    sequenced_local_input.seq = g_sync.next_local_input_seq++;
    sequenced_local_input.payload = std::move(local_input);

    if (g_sync.is_host)
        run_host_step(sequenced_local_input, dt, now);
    else
        run_client_step(sequenced_local_input, dt, now);
    if (g_sync.driver.tick_correction)
        g_sync.driver.tick_correction(g_sync.driver.ctx,
                                      g_sync.member_ids,
                                      g_sync.local_member_id,
                                      g_sync.is_host,
                                      dt);
    return result;
}

const std::string& sync_session_status_text() {
    return g_sync.status_text;
}

const std::string& sync_session_last_error() {
    return g_sync.last_error;
}
