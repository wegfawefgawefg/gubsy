#include "game/menu/lobby_online.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
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

#include "engine/globals.hpp"
#include "engine/mod_host.hpp"
#include "game/menu/lobby_state.hpp"

namespace {

constexpr double kRoomPollIntervalSec = 1.0;
constexpr double kRoomPublishIntervalSec = 1.0;
constexpr double kRoomsRefreshIntervalSec = 2.0;

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

nlohmann::json build_room_metadata(const LobbySession& lobby) {
    nlohmann::json body;
    body["session_name"] = lobby.session_name;
    body["host_name"] = lobby_local_player_name();
    body["privacy"] = lobby.privacy;
    body["max_players"] = lobby.max_players;
    body["game_version"] = required_mod_game_version();
    body["mod_hash"] = lobby_enabled_mod_signature();
    body["in_game"] = lobby.online.in_game;
    return body;
}

void read_room_summary(const nlohmann::json& room_json, LobbyDiscoveredRoom& out) {
    out.room_code = room_json.value("room_code", "");
    out.session_name = room_json.value("session_name", "");
    out.host_name = room_json.value("host_name", "");
    out.game_version = room_json.value("game_version", "");
    out.mod_hash = room_json.value("mod_hash", "");
    out.privacy = room_json.value("privacy", 0);
    out.max_players = room_json.value("max_players", 1);
    out.current_players = room_json.value("current_players", 0);
    out.in_game = room_json.value("in_game", false);
}

void apply_room_to_lobby(const LobbyDiscoveredRoom& room, LobbySession& lobby) {
    lobby.session_name = room.session_name;
    lobby.privacy = room.privacy;
    lobby.max_players = std::max(1, room.max_players);
}

void read_room_members(const nlohmann::json& room_json, LobbySession& lobby) {
    lobby.online.members.clear();
    auto members_it = room_json.find("members");
    if (members_it == room_json.end() || !members_it->is_array())
        return;
    for (const auto& member_json : *members_it) {
        if (!member_json.is_object())
            continue;
        LobbyOnlineMember member;
        member.member_id = member_json.value("member_id", "");
        member.display_name = member_json.value("display_name", "");
        member.is_host = member_json.value("is_host", false);
        member.is_local = member.member_id == lobby.online.member_id;
        if (!member.member_id.empty())
            lobby.online.members.push_back(std::move(member));
    }
}

bool refresh_room_state(LobbySession& lobby, std::string& err) {
    if (!lobby.online.in_room || lobby.online.room_code.empty())
        return false;
    auto json = get_json(lobby.online.server_url,
                         "/rooms/" + normalized_room_code(lobby.online.room_code),
                         err);
    if (!json)
        return false;
    LobbyDiscoveredRoom room;
    read_room_summary((*json)["room"], room);
    apply_room_to_lobby(room, lobby);
    read_room_members((*json)["room"], lobby);
    std::ostringstream status;
    status << "Room " << room.room_code << " | " << room.current_players
           << "/" << room.max_players << " players";
    lobby.online.status_text = status.str();
    return true;
}

bool publish_room_state(LobbySession& lobby, std::string& err) {
    if (!lobby.online.in_room || !lobby.online.is_host)
        return false;
    nlohmann::json body;
    body["member_id"] = lobby.online.member_id;
    body["host_secret"] = lobby.online.host_secret;
    body["display_name"] = lobby_local_player_name();
    body["room"] = build_room_metadata(lobby);
    auto json = post_json(lobby.online.server_url,
                          "/rooms/" + normalized_room_code(lobby.online.room_code) + "/heartbeat",
                          body,
                          err);
    return json.has_value();
}

bool heartbeat_member(LobbySession& lobby, std::string& err) {
    if (!lobby.online.in_room || lobby.online.is_host)
        return false;
    nlohmann::json body;
    body["member_id"] = lobby.online.member_id;
    body["display_name"] = lobby_local_player_name();
    auto json = post_json(lobby.online.server_url,
                          "/rooms/" + normalized_room_code(lobby.online.room_code) + "/heartbeat",
                          body,
                          err);
    return json.has_value();
}

} // namespace

bool lobby_online_host_current_room(LobbySession& lobby, std::string& err) {
    nlohmann::json body = build_room_metadata(lobby);
    auto json = post_json(lobby.online.server_url, "/rooms/create", body, err);
    if (!json)
        return false;
    lobby.online.in_room = true;
    lobby.online.is_host = true;
    lobby.online.room_code = (*json).value("room_code", "");
    lobby.online.host_secret = (*json).value("host_secret", "");
    lobby.online.member_id = (*json).value("member_id", "");
    lobby.online.in_game = false;
    lobby.online.next_room_poll_at = 0.0;
    lobby.online.next_room_publish_at = 0.0;
    err.clear();
    return refresh_room_state(lobby, err);
}

bool lobby_online_join_room(LobbySession& lobby, const std::string& room_code, std::string& err) {
    nlohmann::json body;
    body["display_name"] = lobby_local_player_name();
    std::string code = normalized_room_code(room_code);
    auto json = post_json(lobby.online.server_url, "/rooms/" + code + "/join", body, err);
    if (!json)
        return false;
    lobby.online.in_room = true;
    lobby.online.is_host = false;
    lobby.online.room_code = code;
    lobby.online.host_secret.clear();
    lobby.online.member_id = (*json).value("member_id", "");
    lobby.online.in_game = false;
    lobby.online.next_room_poll_at = 0.0;
    lobby.online.next_room_publish_at = 0.0;
    err.clear();
    return refresh_room_state(lobby, err);
}

bool lobby_online_leave_room(LobbySession& lobby, std::string& err) {
    if (!lobby.online.in_room)
        return true;
    nlohmann::json body;
    body["member_id"] = lobby.online.member_id;
    if (lobby.online.is_host)
        body["host_secret"] = lobby.online.host_secret;
    post_json(lobby.online.server_url,
              "/rooms/" + normalized_room_code(lobby.online.room_code) + "/leave",
              body,
              err);
    lobby.online.in_room = false;
    lobby.online.is_host = false;
    lobby.online.in_game = false;
    lobby.online.room_code.clear();
    lobby.online.host_secret.clear();
    lobby.online.member_id.clear();
    lobby.online.members.clear();
    lobby.online.status_text = "Offline lobby";
    return true;
}

bool lobby_online_refresh_rooms(LobbySession& lobby, bool force, std::string& err) {
    if (!force && es && es->now < lobby.online.next_rooms_refresh_at)
        return true;
    auto json = get_json(lobby.online.server_url, "/rooms", err);
    if (!json)
        return false;
    lobby.online.discovered_rooms.clear();
    auto rooms_it = json->find("rooms");
    if (rooms_it != json->end() && rooms_it->is_array()) {
        for (const auto& room_json : *rooms_it) {
            LobbyDiscoveredRoom room;
            read_room_summary(room_json, room);
            if (!room.room_code.empty())
                lobby.online.discovered_rooms.push_back(std::move(room));
        }
    }
    lobby.online.next_rooms_refresh_at = es ? es->now + kRoomsRefreshIntervalSec : kRoomsRefreshIntervalSec;
    return true;
}

void lobby_online_tick(LobbySession& lobby) {
    if (!es)
        return;
    std::string err;
    if (lobby.online.in_room) {
        if (lobby.online.is_host && es->now >= lobby.online.next_room_publish_at) {
            lobby_refresh_mods();
            if (publish_room_state(lobby, err))
                lobby.online.next_room_publish_at = es->now + kRoomPublishIntervalSec;
            else if (!err.empty())
                lobby.online.last_error = err;
        } else if (!lobby.online.is_host && es->now >= lobby.online.next_room_publish_at) {
            if (heartbeat_member(lobby, err))
                lobby.online.next_room_publish_at = es->now + kRoomPublishIntervalSec;
            else if (!err.empty())
                lobby.online.last_error = err;
        }

        if (es->now >= lobby.online.next_room_poll_at) {
            if (refresh_room_state(lobby, err)) {
                lobby.online.next_room_poll_at = es->now + kRoomPollIntervalSec;
            } else if (!err.empty()) {
                lobby.online.last_error = err;
            }
        }
    }
}
