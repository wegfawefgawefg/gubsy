#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "game/actions.hpp"
#include "game/coop_protocol.hpp"
#include "game/coop_sim.hpp"
#include "game/settings.hpp"
#include "game/state.hpp"

namespace {

struct EndpointInfo {
    std::string host;
    int port{80};
};

bool parse_http_endpoint(const std::string& url, EndpointInfo& out, std::string& err) {
    if (url.rfind("http://", 0) != 0) {
        err = "only http:// URLs are supported";
        return false;
    }
    std::string work = url.substr(7);
    auto slash = work.find('/');
    if (slash != std::string::npos)
        work = work.substr(0, slash);
    out.host = work;
    auto colon = work.find(':');
    if (colon != std::string::npos) {
        out.host = work.substr(0, colon);
        out.port = std::stoi(work.substr(colon + 1));
    }
    return !out.host.empty();
}

nlohmann::json post_json(httplib::Client& client, const std::string& path, const nlohmann::json& body) {
    auto res = client.Post(path.c_str(), body.dump(), "application/json");
    if (!res)
        throw std::runtime_error("request failed for " + path);
    if (res->status < 200 || res->status >= 300)
        throw std::runtime_error("request failed for " + path + ": " + std::to_string(res->status) + " " + res->body);
    return nlohmann::json::parse(res->body);
}

nlohmann::json get_json(httplib::Client& client, const std::string& path) {
    auto res = client.Get(path.c_str());
    if (!res)
        throw std::runtime_error("request failed for " + path);
    if (res->status < 200 || res->status >= 300)
        throw std::runtime_error("request failed for " + path + ": " + std::to_string(res->status) + " " + res->body);
    return nlohmann::json::parse(res->body);
}

void set_down(InputFrame& frame, int action) {
    frame.down_bits |= (1u << action);
}

InputFrame scripted_host_input(int frame_index) {
    InputFrame frame;
    if (frame_index < 36)
        set_down(frame, GameAction::RIGHT);
    if (frame_index == 40)
        set_down(frame, GameAction::USE);
    return frame;
}

InputFrame scripted_guest_input(int frame_index) {
    InputFrame frame;
    if (frame_index < 20)
        set_down(frame, GameAction::RIGHT);
    if (frame_index >= 20 && frame_index < 40)
        set_down(frame, GameAction::DOWN);
    if (frame_index == 18)
        set_down(frame, GameAction::USE);
    return frame;
}

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

bool nearly_equal(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

void compare_states(const State& host, const State& client, const std::vector<std::string>& host_ids, const std::vector<std::string>& client_ids) {
    require(host.players.size() == client.players.size(), "player count mismatch");
    require(host_ids == client_ids, "member order mismatch");
    for (std::size_t i = 0; i < host.players.size(); ++i) {
        require(nearly_equal(host.players[i].pos.x, client.players[i].pos.x), "player x mismatch");
        require(nearly_equal(host.players[i].pos.y, client.players[i].pos.y), "player y mismatch");
    }
    require(nearly_equal(host.bonk.cooldown, client.bonk.cooldown), "bonk cooldown mismatch");
    require(nearly_equal(host.bar_height, client.bar_height), "bar height mismatch");
}

} // namespace

int main(int argc, char** argv) {
    std::string server_url = "http://127.0.0.1:8788";
    if (argc > 1)
        server_url = argv[1];

    EndpointInfo endpoint;
    std::string err;
    if (!parse_http_endpoint(server_url, endpoint, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);

    try {
        nlohmann::json created = post_json(client,
                                           "/rooms/create",
                                           {
                                               {"session_name", "Coop Smoke"},
                                               {"host_name", "Host"},
                                               {"privacy", 2},
                                               {"max_players", 4},
                                               {"game_version", "0.1.0"},
                                               {"mod_hash", "smoke"},
                                               {"in_game", true},
                                           });
        const std::string room_code = created.at("room_code").get<std::string>();
        const std::string host_secret = created.at("host_secret").get<std::string>();
        const std::string host_member_id = created.at("member_id").get<std::string>();

        nlohmann::json joined = post_json(client,
                                          "/rooms/" + room_code + "/join",
                                          {{"display_name", "Guest"}});
        const std::string guest_member_id = joined.at("member_id").get<std::string>();

        State host_state;
        State guest_state;
        std::vector<std::string> host_member_ids{host_member_id, guest_member_id};
        std::vector<std::string> guest_member_ids;
        std::vector<InputFrame> host_current(2);
        std::vector<InputFrame> host_previous(2);

        ensure_demo_player_count(host_state, host_member_ids.size());

        for (int frame_index = 0; frame_index < 90; ++frame_index) {
            const InputFrame host_input = scripted_host_input(frame_index);
            const InputFrame guest_input = scripted_guest_input(frame_index);

            post_json(client,
                      "/rooms/" + room_code + "/input",
                      {
                          {"member_id", guest_member_id},
                          {"input", input_frame_to_json(guest_input)},
                      });

            nlohmann::json inputs_json = get_json(client, "/rooms/" + room_code + "/inputs");
            const auto& members = inputs_json.at("members");
            host_member_ids.clear();
            host_current.clear();
            host_previous.resize(members.size());
            for (std::size_t i = 0; i < members.size(); ++i) {
                const auto& member = members[i];
                host_member_ids.push_back(member.at("member_id").get<std::string>());
                if (host_current.size() <= i)
                    host_current.resize(i + 1);
                if (host_member_ids[i] == host_member_id) {
                    host_current[i] = host_input;
                } else if (auto input_it = member.find("input"); input_it != member.end()) {
                    input_frame_from_json(*input_it, host_current[i]);
                }
            }

            ensure_demo_player_count(host_state, host_member_ids.size());
            simulate_demo_world(host_state, host_current, host_previous, FIXED_TIMESTEP);
            post_json(client,
                      "/rooms/" + room_code + "/snapshot",
                      {
                          {"member_id", host_member_id},
                          {"host_secret", host_secret},
                          {"snapshot", coop_snapshot_to_json(capture_coop_snapshot(host_state, host_member_ids, static_cast<std::uint64_t>(frame_index + 1)))},
                      });

            nlohmann::json snapshot_json = get_json(client, "/rooms/" + room_code + "/snapshot");
            require(snapshot_json.at("has_snapshot").get<bool>(), "missing snapshot");
            CoopStateSnapshot snapshot;
            require(coop_snapshot_from_json(snapshot_json.at("snapshot"), snapshot), "failed to decode snapshot");
            apply_coop_snapshot(snapshot, guest_state, guest_member_ids);

            compare_states(host_state, guest_state, host_member_ids, guest_member_ids);
            host_previous = host_current;
        }

        post_json(client,
                  "/rooms/" + room_code + "/leave",
                  {
                      {"member_id", guest_member_id},
                  });
        post_json(client,
                  "/rooms/" + room_code + "/leave",
                  {
                      {"member_id", host_member_id},
                      {"host_secret", host_secret},
                  });

        std::cout << "[coop_sync_smoke] ok\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[coop_sync_smoke] " << e.what() << "\n";
        return 1;
    }
}
