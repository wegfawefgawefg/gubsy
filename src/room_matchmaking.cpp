#include "src/room_matchmaking.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include <httplib/httplib.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <nlohmann/json.hpp>

#include "src/session_contract.hpp"

namespace {

struct EndpointInfo {
    std::string host;
    int port{80};
};

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

nlohmann::json room_to_json(const MatchmakingRoom& room) {
    nlohmann::json body;
    body["session_name"] = room.session_name;
    body["host_name"] = room.host_name;
    body["privacy"] = room.privacy;
    body["max_players"] = room.max_players;
    nlohmann::json contract_json = session_contract_to_json(room.contract);
    for (auto it = contract_json.begin(); it != contract_json.end(); ++it)
        body[it.key()] = it.value();
    body["in_game"] = session_contract_is_in_game(room.contract);
    return body;
}

void member_from_json(const nlohmann::json& json, MatchmakingMember& out) {
    out.member_id = json.value("member_id", "");
    out.display_name = json.value("display_name", "");
    out.client_label = json.value("client_label", "");
    out.last_seen_seconds_ago = json.value("last_seen_seconds_ago", 0);
    out.is_host = json.value("is_host", false);
}

bool room_from_json(const nlohmann::json& json, MatchmakingRoom& out) {
    if (!json.is_object())
        return false;
    out = MatchmakingRoom{};
    out.room_code = json.value("room_code", "");
    out.session_name = json.value("session_name", "");
    out.host_name = json.value("host_name", "");
    out.privacy = json.value("privacy", 0);
    out.max_players = json.value("max_players", 1);
    out.current_players = json.value("current_players", 0);
    if (!session_contract_from_json(json, out.contract))
        return false;
    auto members_it = json.find("members");
    if (members_it != json.end() && members_it->is_array()) {
        for (const auto& member_json : *members_it) {
            if (!member_json.is_object())
                continue;
            MatchmakingMember member;
            member_from_json(member_json, member);
            if (!member.member_id.empty())
                out.members.push_back(std::move(member));
        }
    }
    return true;
}

} // namespace

bool RoomServerMatchmaking::fetch_capabilities(const std::string& server_url,
                                               RoomServerCapabilities& out,
                                               std::string& err) {
    auto json = get_json(server_url, "/health", err);
    if (!json)
        return false;
    out = RoomServerCapabilities{};
    out.ok = (*json).value("ok", false);
    const auto realnet_it = json->find("realnet");
    if (realnet_it != json->end() && realnet_it->is_object()) {
        const auto rendezvous_it = realnet_it->find("rendezvous_udp");
        if (rendezvous_it != realnet_it->end() && rendezvous_it->is_object()) {
            out.rendezvous_udp.enabled = rendezvous_it->value("enabled", false);
            out.rendezvous_udp.host = rendezvous_it->value("host", "");
            out.rendezvous_udp.port = rendezvous_it->value("port", 0);
            out.rendezvous_udp.protocol = rendezvous_it->value("protocol", "");
        }
    }
    return out.ok;
}

bool RoomServerMatchmaking::create_room(const std::string& server_url,
                                        const MatchmakingRoom& room,
                                        MatchmakingCreateResult& out,
                                        std::string& err) {
    auto json = post_json(server_url, "/rooms/create", room_to_json(room), err);
    if (!json)
        return false;
    out.room_code = (*json).value("room_code", "");
    out.host_secret = (*json).value("host_secret", "");
    out.member_id = (*json).value("member_id", "");
    return true;
}

bool RoomServerMatchmaking::join_room(const std::string& server_url,
                                      const std::string& room_code,
                                      const std::string& display_name,
                                      const std::string& join_token,
                                      std::string& member_id_out,
                                      std::string& err) {
    nlohmann::json body{{"display_name", display_name}};
    if (!join_token.empty())
        body["join_token"] = join_token;
    auto json = post_json(server_url,
                          "/rooms/" + normalized_room_code(room_code) + "/join",
                          body,
                          err);
    if (!json)
        return false;
    member_id_out = (*json).value("member_id", "");
    return true;
}

bool RoomServerMatchmaking::create_join_attempt(const std::string& server_url,
                                                const std::string& room_code,
                                                const std::string& display_name,
                                                MatchmakingJoinAttemptResult& out,
                                                std::string& err) {
    auto json = post_json(server_url,
                          "/rooms/" + normalized_room_code(room_code) + "/join_attempt",
                          {{"display_name", display_name}},
                          err);
    if (!json)
        return false;
    out = MatchmakingJoinAttemptResult{};
    out.join_attempt_id = (*json).value("join_attempt_id", "");
    out.join_token = (*json).value("join_token", "");
    out.punch_secret = (*json).value("punch_secret", "");
    auto room_it = json->find("room");
    if (room_it != json->end())
        (void)room_from_json(*room_it, out.room);
    return true;
}

bool RoomServerMatchmaking::leave_room(const std::string& server_url,
                                       const std::string& room_code,
                                       const std::string& member_id,
                                       const std::string& host_secret,
                                       std::string& err) {
    nlohmann::json body{{"member_id", member_id}};
    if (!host_secret.empty())
        body["host_secret"] = host_secret;
    return post_json(server_url,
                     "/rooms/" + normalized_room_code(room_code) + "/leave",
                     body,
                     err).has_value();
}

bool RoomServerMatchmaking::remove_member(const std::string& server_url,
                                          const std::string& room_code,
                                          const std::string& host_secret,
                                          const std::string& target_member_id,
                                          std::string& err) {
    nlohmann::json body{
        {"host_secret", host_secret},
        {"member_id", target_member_id},
    };
    return post_json(server_url,
                     "/rooms/" + normalized_room_code(room_code) + "/remove_member",
                     body,
                     err).has_value();
}

bool RoomServerMatchmaking::heartbeat_room(const std::string& server_url,
                                           const std::string& room_code,
                                           const std::string& member_id,
                                           const std::string& display_name,
                                           const std::string& host_secret,
                                           const MatchmakingRoom* room_update,
                                           std::string& err) {
    nlohmann::json body{
        {"member_id", member_id},
        {"display_name", display_name},
    };
    if (!host_secret.empty())
        body["host_secret"] = host_secret;
    if (room_update)
        body["room"] = room_to_json(*room_update);
    return post_json(server_url,
                     "/rooms/" + normalized_room_code(room_code) + "/heartbeat",
                     body,
                     err).has_value();
}

bool RoomServerMatchmaking::fetch_room(const std::string& server_url,
                                       const std::string& room_code,
                                       MatchmakingRoom& out,
                                       std::string& err) {
    auto json = get_json(server_url,
                         "/rooms/" + normalized_room_code(room_code),
                         err);
    if (!json)
        return false;
    return room_from_json((*json)["room"], out);
}

bool RoomServerMatchmaking::list_rooms(const std::string& server_url,
                                       std::vector<MatchmakingRoom>& out,
                                       std::string& err) {
    auto json = get_json(server_url, "/rooms", err);
    if (!json)
        return false;
    out.clear();
    auto rooms_it = json->find("rooms");
    if (rooms_it == json->end() || !rooms_it->is_array())
        return true;
    out.reserve(rooms_it->size());
    for (const auto& room_json : *rooms_it) {
        MatchmakingRoom room;
        if (room_from_json(room_json, room))
            out.push_back(std::move(room));
    }
    return true;
}
