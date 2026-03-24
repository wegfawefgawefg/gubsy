#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

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
    if (!res) {
        throw std::runtime_error("request failed for " + path);
    }
    if (res->status < 200 || res->status >= 300) {
        throw std::runtime_error("request failed for " + path + ": " + std::to_string(res->status) + " " + res->body);
    }
    return nlohmann::json::parse(res->body);
}

nlohmann::json get_json(httplib::Client& client, const std::string& path) {
    auto res = client.Get(path.c_str());
    if (!res) {
        throw std::runtime_error("request failed for " + path);
    }
    if (res->status < 200 || res->status >= 300) {
        throw std::runtime_error("request failed for " + path + ": " + std::to_string(res->status) + " " + res->body);
    }
    return nlohmann::json::parse(res->body);
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
                                               {"session_name", "Smoke Lobby"},
                                               {"host_name", "Host"},
                                               {"privacy", 2},
                                               {"max_players", 4},
                                               {"game_version", "0.1.0"},
                                               {"mod_hash", "aaaabbbb"},
                                           });
        std::string room_code = created.at("room_code").get<std::string>();
        std::string host_secret = created.at("host_secret").get<std::string>();
        std::string host_member_id = created.at("member_id").get<std::string>();

        nlohmann::json listed = get_json(client, "/rooms");
        bool found_room = false;
        for (const auto& room : listed.at("rooms")) {
            if (room.at("room_code").get<std::string>() == room_code)
                found_room = true;
        }
        if (!found_room)
            throw std::runtime_error("created room missing from room list");

        nlohmann::json joined = post_json(client,
                                          "/rooms/" + room_code + "/join",
                                          {{"display_name", "Guest"}});
        std::string guest_member_id = joined.at("member_id").get<std::string>();

        post_json(client,
                  "/rooms/" + room_code + "/heartbeat",
                  {
                      {"member_id", host_member_id},
                      {"display_name", "Host"},
                      {"host_secret", host_secret},
                      {"room",
                       {
                           {"session_name", "Smoke Lobby Updated"},
                           {"host_name", "Host"},
                           {"realtime_endpoint", "udp://127.0.0.1:9000"},
                           {"privacy", 4},
                           {"max_players", 6},
                           {"game_version", "0.1.1"},
                           {"mod_hash", "ccccdddd"},
                           {"in_game", false},
                       }},
                  });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        nlohmann::json room = get_json(client, "/rooms/" + room_code);
        const auto& room_json = room.at("room");
        if (room_json.at("session_name").get<std::string>() != "Smoke Lobby Updated")
            throw std::runtime_error("room session_name did not update");
        if (room_json.at("current_players").get<int>() != 2)
            throw std::runtime_error("room player count mismatch");
        if (room_json.at("mod_hash").get<std::string>() != "ccccdddd")
            throw std::runtime_error("room mod hash did not update");
        if (room_json.at("realtime_endpoint").get<std::string>() != "udp://127.0.0.1:9000")
            throw std::runtime_error("room realtime endpoint did not update");

        post_json(client,
                  "/rooms/" + room_code + "/leave",
                  {
                      {"member_id", guest_member_id},
                  });

        nlohmann::json after_leave = get_json(client, "/rooms/" + room_code);
        if (after_leave.at("room").at("current_players").get<int>() != 1)
            throw std::runtime_error("leave did not remove guest");

        post_json(client,
                  "/rooms/" + room_code + "/leave",
                  {
                      {"member_id", host_member_id},
                      {"host_secret", host_secret},
                  });

        auto res = client.Get(("/rooms/" + room_code).c_str());
        if (res && res->status != 404)
            throw std::runtime_error("host leave should remove room");

        std::cout << "[room_smoke] ok\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[room_smoke] " << e.what() << "\n";
        return 1;
    }
}
