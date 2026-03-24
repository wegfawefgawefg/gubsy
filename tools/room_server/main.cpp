#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr auto kMemberTimeout = std::chrono::seconds(12);
constexpr auto kCodeAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr int kRoomCodeLen = 6;
constexpr int kMemberIdLen = 8;
constexpr int kSecretLen = 24;

struct RoomMember {
    std::string member_id;
    std::string display_name;
    bool is_host{false};
    Clock::time_point last_seen{Clock::now()};
};

struct RoomRecord {
    std::string room_code;
    std::string host_secret;
    std::string session_name;
    std::string host_name;
    std::string game_version;
    std::string mod_hash;
    int privacy{0};
    int max_players{1};
    bool in_game{false};
    std::vector<RoomMember> members;
    Clock::time_point created_at{Clock::now()};
    Clock::time_point updated_at{Clock::now()};
};

struct RoomRegistry {
    std::mutex mutex;
    std::unordered_map<std::string, RoomRecord> rooms;
    std::mt19937_64 rng{std::random_device{}()};

    std::string random_token(int len) {
        std::uniform_int_distribution<std::size_t> dist(0, std::char_traits<char>::length(kCodeAlphabet) - 1);
        std::string out;
        out.reserve(static_cast<std::size_t>(len));
        for (int i = 0; i < len; ++i)
            out.push_back(kCodeAlphabet[dist(rng)]);
        return out;
    }

    std::string unique_room_code() {
        for (;;) {
            std::string code = random_token(kRoomCodeLen);
            if (!rooms.count(code))
                return code;
        }
    }

    void cleanup_expired_locked() {
        const auto now = Clock::now();
        std::vector<std::string> dead_rooms;
        for (auto& [room_code, room] : rooms) {
            room.members.erase(std::remove_if(room.members.begin(),
                                              room.members.end(),
                                              [&](const RoomMember& member) {
                                                  return now - member.last_seen > kMemberTimeout;
                                              }),
                               room.members.end());
            auto host_it = std::find_if(room.members.begin(), room.members.end(),
                                        [](const RoomMember& member) { return member.is_host; });
            if (room.members.empty() || host_it == room.members.end())
                dead_rooms.push_back(room_code);
        }
        for (const auto& room_code : dead_rooms)
            rooms.erase(room_code);
    }
};

RoomRegistry g_registry;

RoomMember* find_member(RoomRecord& room, const std::string& member_id) {
    for (auto& member : room.members) {
        if (member.member_id == member_id)
            return &member;
    }
    return nullptr;
}

nlohmann::json room_to_json(const RoomRecord& room) {
    nlohmann::json members = nlohmann::json::array();
    for (const auto& member : room.members) {
        members.push_back({
            {"member_id", member.member_id},
            {"display_name", member.display_name},
            {"is_host", member.is_host},
        });
    }
    return {
        {"room_code", room.room_code},
        {"session_name", room.session_name},
        {"host_name", room.host_name},
        {"game_version", room.game_version},
        {"mod_hash", room.mod_hash},
        {"privacy", room.privacy},
        {"max_players", room.max_players},
        {"current_players", static_cast<int>(room.members.size())},
        {"in_game", room.in_game},
        {"members", std::move(members)},
    };
}

bool read_body_json(const httplib::Request& req, nlohmann::json& body, httplib::Response& res) {
    try {
        body = nlohmann::json::parse(req.body);
        return true;
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(std::string("invalid json: ") + e.what(), "text/plain");
        return false;
    }
}

std::string json_string(const nlohmann::json& body, const char* key, const char* fallback = "") {
    auto it = body.find(key);
    if (it != body.end() && it->is_string())
        return it->get<std::string>();
    return fallback;
}

int json_int(const nlohmann::json& body, const char* key, int fallback = 0) {
    auto it = body.find(key);
    if (it != body.end() && it->is_number_integer())
        return it->get<int>();
    return fallback;
}

bool json_bool(const nlohmann::json& body, const char* key, bool fallback = false) {
    auto it = body.find(key);
    if (it != body.end() && it->is_boolean())
        return it->get<bool>();
    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    int port = 8788;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--port=", 0) == 0) {
            try {
                port = std::stoi(arg.substr(7));
            } catch (...) {
                port = 8788;
            }
        } else if (arg == "--help") {
            std::cout << "Usage: room_server [--port=<port>]\n";
            return 0;
        }
    }

    httplib::Server server;

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"ok":true})", "application/json");
    });

    server.Get("/rooms", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();
        nlohmann::json rooms = nlohmann::json::array();
        for (const auto& [_, room] : g_registry.rooms)
            rooms.push_back(room_to_json(room));
        res.set_content(nlohmann::json{{"rooms", std::move(rooms)}}.dump(), "application/json");
    });

    server.Get(R"(/rooms/([A-Z0-9]+))", [](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();
        auto it = g_registry.rooms.find(req.matches[1].str());
        if (it == g_registry.rooms.end()) {
            res.status = 404;
            res.set_content("room not found", "text/plain");
            return;
        }
        res.set_content(nlohmann::json{{"room", room_to_json(it->second)}}.dump(), "application/json");
    });

    server.Post("/rooms/create", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        if (!read_body_json(req, body, res))
            return;
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();

        RoomRecord room;
        room.room_code = g_registry.unique_room_code();
        room.host_secret = g_registry.random_token(kSecretLen);
        room.session_name = json_string(body, "session_name", "Online Lobby");
        room.host_name = json_string(body, "host_name", "Host");
        room.game_version = json_string(body, "game_version");
        room.mod_hash = json_string(body, "mod_hash");
        room.privacy = json_int(body, "privacy", 0);
        room.max_players = std::max(1, json_int(body, "max_players", 4));
        room.in_game = json_bool(body, "in_game", false);

        RoomMember host;
        host.member_id = g_registry.random_token(kMemberIdLen);
        host.display_name = room.host_name;
        host.is_host = true;
        room.members.push_back(std::move(host));

        const std::string member_id = room.members.front().member_id;
        g_registry.rooms.emplace(room.room_code, room);

        res.set_content(nlohmann::json{
                            {"room_code", room.room_code},
                            {"host_secret", room.host_secret},
                            {"member_id", member_id},
                        }.dump(),
                        "application/json");
    });

    server.Post(R"(/rooms/([A-Z0-9]+)/join)", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        if (!read_body_json(req, body, res))
            return;
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();

        auto it = g_registry.rooms.find(req.matches[1].str());
        if (it == g_registry.rooms.end()) {
            res.status = 404;
            res.set_content("room not found", "text/plain");
            return;
        }
        RoomRecord& room = it->second;
        if (static_cast<int>(room.members.size()) >= room.max_players) {
            res.status = 409;
            res.set_content("room is full", "text/plain");
            return;
        }

        RoomMember member;
        member.member_id = g_registry.random_token(kMemberIdLen);
        member.display_name = json_string(body, "display_name", "Guest");
        room.members.push_back(member);
        room.updated_at = Clock::now();

        res.set_content(nlohmann::json{
                            {"member_id", member.member_id},
                            {"room", room_to_json(room)},
                        }.dump(),
                        "application/json");
    });

    server.Post(R"(/rooms/([A-Z0-9]+)/heartbeat)", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        if (!read_body_json(req, body, res))
            return;
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();

        auto it = g_registry.rooms.find(req.matches[1].str());
        if (it == g_registry.rooms.end()) {
            res.status = 404;
            res.set_content("room not found", "text/plain");
            return;
        }
        RoomRecord& room = it->second;
        RoomMember* member = find_member(room, json_string(body, "member_id"));
        if (!member) {
            res.status = 404;
            res.set_content("member not found", "text/plain");
            return;
        }

        member->display_name = json_string(body, "display_name", member->display_name.c_str());
        member->last_seen = Clock::now();

        const std::string host_secret = json_string(body, "host_secret");
        if (member->is_host && !host_secret.empty() && host_secret == room.host_secret) {
            auto room_it = body.find("room");
            if (room_it != body.end() && room_it->is_object()) {
                room.session_name = json_string(*room_it, "session_name", room.session_name.c_str());
                room.host_name = json_string(*room_it, "host_name", room.host_name.c_str());
                room.game_version = json_string(*room_it, "game_version", room.game_version.c_str());
                room.mod_hash = json_string(*room_it, "mod_hash", room.mod_hash.c_str());
                room.privacy = json_int(*room_it, "privacy", room.privacy);
                room.max_players = std::max(1, json_int(*room_it, "max_players", room.max_players));
                room.in_game = json_bool(*room_it, "in_game", room.in_game);
                member->display_name = room.host_name;
            }
        }

        room.updated_at = Clock::now();
        res.set_content(nlohmann::json{{"ok", true}}.dump(), "application/json");
    });

    server.Post(R"(/rooms/([A-Z0-9]+)/leave)", [](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body;
        if (!read_body_json(req, body, res))
            return;
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();

        auto it = g_registry.rooms.find(req.matches[1].str());
        if (it == g_registry.rooms.end()) {
            res.set_content(R"({"ok":true})", "application/json");
            return;
        }
        RoomRecord& room = it->second;
        const std::string member_id = json_string(body, "member_id");
        const std::string host_secret = json_string(body, "host_secret");

        auto member_it = std::find_if(room.members.begin(), room.members.end(),
                                      [&](const RoomMember& member) { return member.member_id == member_id; });
        if (member_it != room.members.end()) {
            const bool is_host = member_it->is_host;
            room.members.erase(member_it);
            if (is_host || host_secret == room.host_secret)
                g_registry.rooms.erase(it);
        }
        res.set_content(R"({"ok":true})", "application/json");
    });

    std::cout << "[room_server] Listening on 127.0.0.1:" << port << "\n";
    server.listen("127.0.0.1", port);
    return 0;
}
