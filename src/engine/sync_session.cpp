#include "engine/sync_session.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include <httplib/httplib.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

constexpr double kInputPushIntervalSec = 1.0 / 20.0;
constexpr double kInputPollIntervalSec = 1.0 / 20.0;
constexpr double kSnapshotPushIntervalSec = 1.0 / 20.0;
constexpr double kSnapshotPollIntervalSec = 1.0 / 20.0;

struct EndpointInfo {
    std::string host;
    int port{80};
};

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
    std::uint64_t sim_frame{0};
    std::uint64_t last_applied_snapshot_frame{0};
    double next_input_push_at{0.0};
    double next_input_poll_at{0.0};
    double next_snapshot_push_at{0.0};
    double next_snapshot_poll_at{0.0};
};

SyncRuntime g_sync;

bool parse_http_endpoint(const std::string& url, EndpointInfo& out, std::string& err) {
    std::string work = url;
    constexpr const char* prefix = "http://";
    if (work.rfind(prefix, 0) != 0) {
        err = "Only http:// room servers are supported";
        return false;
    }
    work = work.substr(7);
    auto slash = work.find('/');
    if (slash != std::string::npos)
        work = work.substr(0, slash);
    if (work.empty()) {
        err = "Missing host in room server URL";
        return false;
    }
    out.host = work;
    auto colon = work.find(':');
    if (colon != std::string::npos) {
        out.host = work.substr(0, colon);
        try {
            out.port = std::stoi(work.substr(colon + 1));
        } catch (...) {
            err = "Invalid room server port";
            return false;
        }
    }
    if (out.host.empty()) {
        err = "Invalid room server host";
        return false;
    }
    return true;
}

std::optional<nlohmann::json> post_json(const std::string& server_url,
                                        const std::string& path,
                                        const nlohmann::json& body,
                                        std::string& err) {
    EndpointInfo endpoint;
    if (!parse_http_endpoint(server_url, endpoint, err))
        return std::nullopt;
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);
    auto res = client.Post(path.c_str(), body.dump(), "application/json");
    if (!res) {
        err = "Failed to reach room server";
        return std::nullopt;
    }
    if (res->status < 200 || res->status >= 300) {
        err = "Room server request failed (" + std::to_string(res->status) + ")";
        if (!res->body.empty())
            err += ": " + res->body;
        return std::nullopt;
    }
    try {
        return nlohmann::json::parse(res->body);
    } catch (const std::exception& e) {
        err = e.what();
        return std::nullopt;
    }
}

std::optional<nlohmann::json> get_json(const std::string& server_url,
                                       const std::string& path,
                                       std::string& err) {
    EndpointInfo endpoint;
    if (!parse_http_endpoint(server_url, endpoint, err))
        return std::nullopt;
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);
    auto res = client.Get(path.c_str());
    if (!res) {
        err = "Failed to reach room server";
        return std::nullopt;
    }
    if (res->status < 200 || res->status >= 300) {
        err = "Room server request failed (" + std::to_string(res->status) + ")";
        if (!res->body.empty())
            err += ": " + res->body;
        return std::nullopt;
    }
    try {
        return nlohmann::json::parse(res->body);
    } catch (const std::exception& e) {
        err = e.what();
        return std::nullopt;
    }
}

std::string normalized_room_code(std::string room_code) {
    room_code.erase(std::remove_if(room_code.begin(),
                                   room_code.end(),
                                   [](unsigned char c) { return std::isspace(c) != 0; }),
                    room_code.end());
    std::transform(room_code.begin(), room_code.end(), room_code.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return room_code;
}

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
    for (std::size_t i = 0; i < g_sync.member_ids.size(); ++i) {
        current_by_member[g_sync.member_ids[i]] = g_sync.current_inputs[i];
        previous_by_member[g_sync.member_ids[i]] = g_sync.previous_inputs[i];
    }

    g_sync.member_ids = member_ids;
    g_sync.current_inputs.assign(member_ids.size(), nlohmann::json::object());
    g_sync.previous_inputs.assign(member_ids.size(), nlohmann::json::object());
    for (std::size_t i = 0; i < member_ids.size(); ++i) {
        auto current_it = current_by_member.find(member_ids[i]);
        if (current_it != current_by_member.end())
            g_sync.current_inputs[i] = current_it->second;
        auto previous_it = previous_by_member.find(member_ids[i]);
        if (previous_it != previous_by_member.end())
            g_sync.previous_inputs[i] = previous_it->second;
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
    next.room_code = normalized_room_code(connection.room_code);
    next.host_secret = connection.host_secret;
    next.local_member_id = connection.local_member_id;
    next.has_authoritative_snapshot = false;
    next.member_ids.clear();
    next.current_inputs.clear();
    next.previous_inputs.clear();
    next.sim_frame = 0;
    next.last_applied_snapshot_frame = 0;
    next.next_input_push_at = 0.0;
    next.next_input_poll_at = 0.0;
    next.next_snapshot_push_at = 0.0;
    next.next_snapshot_poll_at = 0.0;
    g_sync = std::move(next);
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

    const std::string normalized_code = normalized_room_code(connection.room_code);
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

bool publish_local_input(const nlohmann::json& input, std::string& err) {
    nlohmann::json body;
    body["member_id"] = g_sync.local_member_id;
    body["input"] = input;
    auto json = post_json(g_sync.server_url,
                          "/rooms/" + g_sync.room_code + "/input",
                          body,
                          err);
    return json.has_value();
}

bool fetch_room_inputs(std::string& err) {
    auto json = get_json(g_sync.server_url,
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
        if (input_it != member_json.end() && input_it->is_object())
            g_sync.current_inputs[static_cast<std::size_t>(index)] = *input_it;
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

    nlohmann::json body;
    body["member_id"] = g_sync.local_member_id;
    body["host_secret"] = g_sync.host_secret;
    body["snapshot"] = std::move(snapshot);
    auto json = post_json(g_sync.server_url,
                          "/rooms/" + g_sync.room_code + "/snapshot",
                          body,
                          err);
    return json.has_value();
}

bool fetch_snapshot(std::string& err) {
    if (!g_sync.driver.apply_snapshot)
        return false;
    auto json = get_json(g_sync.server_url,
                         "/rooms/" + g_sync.room_code + "/snapshot",
                         err);
    if (!json)
        return false;
    if (!json->value("has_snapshot", false))
        return true;

    const nlohmann::json& snapshot = (*json)["snapshot"];
    const std::uint64_t sim_frame = snapshot.value("sim_frame", std::uint64_t{0});
    if (sim_frame <= g_sync.last_applied_snapshot_frame)
        return true;

    std::vector<std::string> member_ids;
    if (!g_sync.driver.apply_snapshot(g_sync.driver.ctx, snapshot, member_ids))
        return false;
    rebuild_member_buffers(member_ids);
    g_sync.last_applied_snapshot_frame = sim_frame;
    g_sync.has_authoritative_snapshot = true;
    return true;
}

void run_host_step(const nlohmann::json& local_input, float dt, double now) {
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

    g_sync.current_inputs[static_cast<std::size_t>(local_index)] = local_input;
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
    g_sync.sim_frame += 1;
}

void run_client_step(const nlohmann::json& local_input, float dt, double now) {
    int local_index = find_member_index(g_sync, g_sync.local_member_id);
    if (local_index < 0) {
        std::vector<std::string> member_ids = g_sync.member_ids;
        member_ids.push_back(g_sync.local_member_id);
        rebuild_member_buffers(member_ids);
        local_index = static_cast<int>(g_sync.member_ids.size()) - 1;
    }

    g_sync.current_inputs[static_cast<std::size_t>(local_index)] = local_input;
    if (g_sync.has_authoritative_snapshot && g_sync.driver.predict) {
        g_sync.driver.predict(g_sync.driver.ctx,
                              g_sync.member_ids,
                              g_sync.current_inputs,
                              g_sync.previous_inputs,
                              dt);
    }
    if (g_sync.driver.apply_local_view_input)
        g_sync.driver.apply_local_view_input(g_sync.driver.ctx, local_input);

    if (now >= g_sync.next_input_push_at) {
        std::string err;
        if (publish_local_input(local_input, err))
            g_sync.next_input_push_at = now + kInputPushIntervalSec;
        else if (!err.empty())
            g_sync.last_error = err;
    }

    if (now >= g_sync.next_snapshot_poll_at) {
        std::string err;
        if (fetch_snapshot(err))
            g_sync.next_snapshot_poll_at = now + kSnapshotPollIntervalSec;
        else if (!err.empty())
            g_sync.last_error = err;
    }

    g_sync.previous_inputs = g_sync.current_inputs;
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
    g_sync.sim_frame = 0;
    g_sync.last_applied_snapshot_frame = 0;
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

    if (g_sync.is_host)
        run_host_step(local_input, dt, now);
    else
        run_client_step(local_input, dt, now);

    return result;
}

const std::string& sync_session_status_text() {
    return g_sync.status_text;
}

const std::string& sync_session_last_error() {
    return g_sync.last_error;
}
