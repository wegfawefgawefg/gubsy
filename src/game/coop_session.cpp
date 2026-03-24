#include "game/coop_session.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
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
#include <nlohmann/json.hpp>

#include "engine/globals.hpp"
#include "game/coop_protocol.hpp"
#include "game/coop_sim.hpp"
#include "game/input_frame.hpp"
#include "game/menu/lobby_online.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/settings.hpp"
#include "game/state.hpp"

namespace {

constexpr double kInputPushIntervalSec = 1.0 / 20.0;
constexpr double kInputPollIntervalSec = 1.0 / 20.0;
constexpr double kSnapshotPushIntervalSec = 1.0 / 20.0;
constexpr double kSnapshotPollIntervalSec = 1.0 / 20.0;

struct EndpointInfo {
    std::string host;
    int port{80};
};

struct CoopRuntime {
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
    std::vector<InputFrame> current_inputs;
    std::vector<InputFrame> previous_inputs;
    std::uint64_t sim_frame{0};
    std::uint64_t last_applied_snapshot_frame{0};
    double next_input_push_at{0.0};
    double next_input_poll_at{0.0};
    double next_snapshot_push_at{0.0};
    double next_snapshot_poll_at{0.0};
};

CoopRuntime g_coop;

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

int find_member_index(const CoopRuntime& runtime, const std::string& member_id) {
    for (std::size_t i = 0; i < runtime.member_ids.size(); ++i) {
        if (runtime.member_ids[i] == member_id)
            return static_cast<int>(i);
    }
    return -1;
}

void rebuild_member_buffers(const std::vector<std::string>& member_ids) {
    std::unordered_map<std::string, InputFrame> current_by_member;
    std::unordered_map<std::string, InputFrame> previous_by_member;
    for (std::size_t i = 0; i < g_coop.member_ids.size(); ++i) {
        current_by_member[g_coop.member_ids[i]] = g_coop.current_inputs[i];
        previous_by_member[g_coop.member_ids[i]] = g_coop.previous_inputs[i];
    }

    g_coop.member_ids = member_ids;
    g_coop.current_inputs.assign(member_ids.size(), InputFrame{});
    g_coop.previous_inputs.assign(member_ids.size(), InputFrame{});
    for (std::size_t i = 0; i < member_ids.size(); ++i) {
        auto current_it = current_by_member.find(member_ids[i]);
        if (current_it != current_by_member.end())
            g_coop.current_inputs[i] = current_it->second;
        auto previous_it = previous_by_member.find(member_ids[i]);
        if (previous_it != previous_by_member.end())
            g_coop.previous_inputs[i] = previous_it->second;
    }
}

void rebuild_member_buffers_from_lobby(const LobbySession& lobby) {
    std::vector<std::string> member_ids;
    member_ids.reserve(lobby.online.members.size() + 1);
    for (const auto& member : lobby.online.members) {
        if (!member.member_id.empty())
            member_ids.push_back(member.member_id);
    }
    if (!g_coop.local_member_id.empty() &&
        std::find(member_ids.begin(), member_ids.end(), g_coop.local_member_id) == member_ids.end()) {
        member_ids.push_back(g_coop.local_member_id);
    }
    if (member_ids.empty() && !g_coop.local_member_id.empty())
        member_ids.push_back(g_coop.local_member_id);
    rebuild_member_buffers(member_ids);
}

bool build_local_input(InputFrame& out) {
    if (!es)
        return false;
    build_input_frame(0, es->device_state, out);
    return true;
}

void set_status_line() {
    const std::size_t remote_count = g_coop.member_ids.size();
    if (!g_coop.active) {
        g_coop.status_text = "Offline";
        return;
    }
    g_coop.status_text = std::string(g_coop.is_host ? "Online Host" : "Online Client") +
                         " | room " + g_coop.room_code +
                         " | peers " + std::to_string(remote_count);
}

void start_from_lobby(const LobbySession& lobby) {
    g_coop = CoopRuntime{};
    g_coop.active = true;
    g_coop.is_host = lobby.online.is_host;
    g_coop.server_url = lobby.online.server_url;
    g_coop.room_code = normalized_room_code(lobby.online.room_code);
    g_coop.host_secret = lobby.online.host_secret;
    g_coop.local_member_id = lobby.online.member_id;
    rebuild_member_buffers_from_lobby(lobby);
    if (g_coop.is_host && ss)
        ensure_demo_player_count(*ss, std::max<std::size_t>(1, g_coop.member_ids.size()));
    set_status_line();
}

bool ensure_session_started() {
    LobbySession& lobby = lobby_state();
    if (!lobby.online.in_room || !lobby.online.in_game) {
        coop_session_reset();
        return false;
    }

    if (!g_coop.active ||
        g_coop.room_code != normalized_room_code(lobby.online.room_code) ||
        g_coop.local_member_id != lobby.online.member_id ||
        g_coop.is_host != lobby.online.is_host) {
        start_from_lobby(lobby);
    } else if (g_coop.member_ids.empty()) {
        rebuild_member_buffers_from_lobby(lobby);
    }

    set_status_line();
    return g_coop.active;
}

bool publish_local_input(const InputFrame& frame, std::string& err) {
    if (!g_coop.active)
        return false;
    nlohmann::json body;
    body["member_id"] = g_coop.local_member_id;
    body["input"] = input_frame_to_json(frame);
    auto json = post_json(g_coop.server_url,
                          "/rooms/" + g_coop.room_code + "/input",
                          body,
                          err);
    return json.has_value();
}

bool fetch_room_inputs(std::string& err) {
    auto json = get_json(g_coop.server_url,
                         "/rooms/" + g_coop.room_code + "/inputs",
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
        int index = find_member_index(g_coop, member_id);
        if (index < 0)
            continue;
        auto input_it = member_json.find("input");
        if (input_it != member_json.end() && input_it->is_object())
            input_frame_from_json(*input_it, g_coop.current_inputs[static_cast<std::size_t>(index)]);
    }
    set_status_line();
    return true;
}

bool publish_snapshot(std::string& err) {
    if (!ss)
        return false;
    nlohmann::json body;
    body["member_id"] = g_coop.local_member_id;
    body["host_secret"] = g_coop.host_secret;
    body["snapshot"] = coop_snapshot_to_json(capture_coop_snapshot(*ss, g_coop.member_ids, g_coop.sim_frame));
    auto json = post_json(g_coop.server_url,
                          "/rooms/" + g_coop.room_code + "/snapshot",
                          body,
                          err);
    return json.has_value();
}

bool fetch_snapshot(CoopStepResult& result, const InputFrame& local_frame, std::string& err) {
    auto json = get_json(g_coop.server_url,
                         "/rooms/" + g_coop.room_code + "/snapshot",
                         err);
    if (!json)
        return false;
    if (!json->value("has_snapshot", false))
        return true;

    CoopStateSnapshot snapshot;
    if (!coop_snapshot_from_json((*json)["snapshot"], snapshot))
        return false;
    if (snapshot.sim_frame <= g_coop.last_applied_snapshot_frame)
        return true;

    const float previous_cooldown = ss ? ss->bonk.cooldown : 0.0f;
    std::vector<std::string> member_ids;
    if (ss)
        apply_coop_snapshot(snapshot, *ss, member_ids);
    rebuild_member_buffers(member_ids);
    g_coop.last_applied_snapshot_frame = snapshot.sim_frame;
    g_coop.has_authoritative_snapshot = true;

    if (ss) {
        const int local_index = find_member_index(g_coop, g_coop.local_member_id);
        if (local_index >= 0)
            apply_demo_view_input(*ss, local_frame);
        if (previous_cooldown <= 0.0f && ss->bonk.cooldown > 0.0f)
            result.bonk_count += 1;
    }
    return true;
}

void run_host_step(CoopStepResult& result, const InputFrame& local_frame) {
    if (!ss || !es)
        return;

    if (es->now >= g_coop.next_input_poll_at) {
        std::string err;
        if (fetch_room_inputs(err))
            g_coop.next_input_poll_at = es->now + kInputPollIntervalSec;
        else if (!err.empty())
            g_coop.last_error = err;
    }

    int local_index = find_member_index(g_coop, g_coop.local_member_id);
    if (local_index < 0) {
        std::vector<std::string> member_ids = g_coop.member_ids;
        member_ids.insert(member_ids.begin(), g_coop.local_member_id);
        rebuild_member_buffers(member_ids);
        local_index = 0;
    }

    ensure_demo_player_count(*ss, std::max<std::size_t>(1, g_coop.member_ids.size()));
    g_coop.current_inputs[static_cast<std::size_t>(local_index)] = local_frame;
    const CoopSimEvents events = simulate_demo_world(*ss, g_coop.current_inputs, g_coop.previous_inputs, FIXED_TIMESTEP);
    apply_demo_view_input(*ss, local_frame);
    result.bonk_count += events.bonk_count;

    if (es->now >= g_coop.next_snapshot_push_at) {
        std::string err;
        if (publish_snapshot(err))
            g_coop.next_snapshot_push_at = es->now + kSnapshotPushIntervalSec;
        else if (!err.empty())
            g_coop.last_error = err;
    }

    g_coop.previous_inputs = g_coop.current_inputs;
    g_coop.sim_frame += 1;
}

void run_client_step(CoopStepResult& result, const InputFrame& local_frame) {
    if (!ss || !es)
        return;

    int local_index = find_member_index(g_coop, g_coop.local_member_id);
    if (local_index < 0) {
        std::vector<std::string> member_ids = g_coop.member_ids;
        member_ids.push_back(g_coop.local_member_id);
        rebuild_member_buffers(member_ids);
        local_index = static_cast<int>(g_coop.member_ids.size()) - 1;
    }

    g_coop.current_inputs[static_cast<std::size_t>(local_index)] = local_frame;
    if (g_coop.has_authoritative_snapshot) {
        ensure_demo_player_count(*ss, std::max<std::size_t>(1, g_coop.member_ids.size()));
        simulate_demo_world(*ss, g_coop.current_inputs, g_coop.previous_inputs, FIXED_TIMESTEP);
        apply_demo_view_input(*ss, local_frame);
    }

    if (es->now >= g_coop.next_input_push_at) {
        std::string err;
        if (publish_local_input(local_frame, err))
            g_coop.next_input_push_at = es->now + kInputPushIntervalSec;
        else if (!err.empty())
            g_coop.last_error = err;
    }

    if (es->now >= g_coop.next_snapshot_poll_at) {
        std::string err;
        if (fetch_snapshot(result, local_frame, err))
            g_coop.next_snapshot_poll_at = es->now + kSnapshotPollIntervalSec;
        else if (!err.empty())
            g_coop.last_error = err;
    }

    g_coop.previous_inputs = g_coop.current_inputs;
}

} // namespace

void coop_session_reset() {
    g_coop = CoopRuntime{};
    g_coop.status_text = "Offline";
}

bool coop_session_active() {
    return g_coop.active;
}

CoopStepResult coop_session_step() {
    CoopStepResult result;
    LobbySession& lobby = lobby_state();
    if (!ensure_session_started())
        return result;

    lobby_online_tick(lobby);
    if (g_coop.member_ids.empty())
        rebuild_member_buffers_from_lobby(lobby);

    InputFrame local_frame;
    if (!build_local_input(local_frame))
        return result;

    result.handled = true;
    if (g_coop.is_host)
        run_host_step(result, local_frame);
    else
        run_client_step(result, local_frame);

    return result;
}

const std::string& coop_session_status_text() {
    return g_coop.status_text;
}

const std::string& coop_session_last_error() {
    return g_coop.last_error;
}
