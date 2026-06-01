#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "gubsy/realnet/rendezvous.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr auto kMemberTimeout = std::chrono::seconds(12);
constexpr auto kCodeAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr int kRoomCodeLen = 6;
constexpr int kMemberIdLen = 8;
constexpr int kSecretLen = 24;
constexpr int kJoinAttemptIdLen = 10;
constexpr int kDefaultPort = 8788;
constexpr int kDefaultRendezvousPortOffset = 1;
constexpr auto kJoinAttemptTimeout = std::chrono::seconds(30);

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

void close_socket(SocketHandle socket) {
    if (socket == kInvalidSocket)
        return;
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
}

struct RoomMember {
    std::string member_id;
    std::string display_name;
    bool is_host{false};
    Clock::time_point last_seen{Clock::now()};
};

struct JoinAttempt {
    std::string attempt_id;
    std::string token;
    std::string punch_secret;
    std::string display_name;
    std::string joiner_observed_endpoint;
    sockaddr_storage joiner_sockaddr{};
    socklen_t joiner_sockaddr_len{0};
    bool has_joiner_endpoint{false};
    Clock::time_point created_at{Clock::now()};
    Clock::time_point joiner_seen_at{};
};

struct RoomRecord {
    std::string room_code;
    std::string host_secret;
    std::string session_name;
    std::string host_name;
    std::string session_phase{"lobby"};
    std::string authority_mode{"player_host"};
    std::string net_protocol{"gubsy-sync-1"};
    std::string realtime_endpoint;
    nlohmann::json connection_candidates = nlohmann::json::array();
    std::string game_version;
    std::string mod_hash;
    std::vector<std::string> required_mod_ids;
    std::uint64_t content_revision{1};
    bool allow_live_mod_reload{true};
    int privacy{0};
    int max_players{1};
    bool in_game{false};
    nlohmann::json game_config;
    std::vector<RoomMember> members;
    std::unordered_map<std::string, JoinAttempt> join_attempts;
    std::string host_observed_endpoint;
    sockaddr_storage host_sockaddr{};
    socklen_t host_sockaddr_len{0};
    bool has_host_rendezvous_endpoint{false};
    Clock::time_point created_at{Clock::now()};
    Clock::time_point updated_at{Clock::now()};
    Clock::time_point host_rendezvous_seen_at{};
};

bool room_is_public(const RoomRecord& room) {
    return room.privacy > 0;
}

void log_event(const char* event, const nlohmann::json& fields = nlohmann::json::object());

struct RoomRegistry {
    std::mutex mutex;
    std::unordered_map<std::string, RoomRecord> rooms;
    std::mt19937_64 rng{std::random_device{}()};

    std::string random_token(int len) {
        std::uniform_int_distribution<std::size_t> dist(
            0, std::char_traits<char>::length(kCodeAlphabet) - 1);
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
            for (auto it = room.join_attempts.begin(); it != room.join_attempts.end();) {
                if (now - it->second.created_at > kJoinAttemptTimeout) {
                    log_event("punch_attempt_expire",
                              {{"room_code", room_code},
                               {"join_attempt_id", it->second.attempt_id}});
                    it = room.join_attempts.erase(it);
                } else {
                    ++it;
                }
            }
            std::vector<std::string> dead_members;
            room.members.erase(std::remove_if(room.members.begin(), room.members.end(),
                                              [&](const RoomMember& member) {
                                                  const bool expired =
                                                      now - member.last_seen > kMemberTimeout;
                                                  return expired;
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

std::string sockaddr_to_endpoint(const sockaddr_storage& storage, socklen_t len) {
    char host[NI_MAXHOST]{};
    char service[NI_MAXSERV]{};
    const int rc = getnameinfo(reinterpret_cast<const sockaddr*>(&storage), len, host, sizeof(host),
                               service, sizeof(service),
                               NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
        return {};
    return std::string(host) + ":" + service;
}

realnet::Endpoint sockaddr_to_realnet_endpoint(const sockaddr_storage& storage, socklen_t len) {
    const std::string text = sockaddr_to_endpoint(storage, len);
    const auto parsed = realnet::parse_endpoint(text);
    return parsed.value_or(realnet::Endpoint{});
}

JoinAttempt* find_join_attempt_by_id(RoomRecord& room, const std::string& attempt_id) {
    for (auto& [_, attempt] : room.join_attempts) {
        if (attempt.attempt_id == attempt_id)
            return &attempt;
    }
    return nullptr;
}

std::uint64_t ms_since_epoch() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count());
}

void log_event(const char* event, const nlohmann::json& fields) {
    nlohmann::json line = fields.is_object() ? fields : nlohmann::json::object();
    line["event"] = event;
    line["ts_ms"] = ms_since_epoch();
    std::cout << line.dump() << '\n';
}

RoomMember* find_member(RoomRecord& room, const std::string& member_id) {
    for (auto& member : room.members) {
        if (member.member_id == member_id)
            return &member;
    }
    return nullptr;
}

nlohmann::json room_to_json(const RoomRecord& room) {
    nlohmann::json members = nlohmann::json::array();
    const auto now = Clock::now();
    for (const auto& member : room.members) {
        const int last_seen_seconds_ago = std::max(
            0,
            static_cast<int>(
                std::chrono::duration_cast<std::chrono::seconds>(now - member.last_seen).count()));
        members.push_back({
            {"member_id", member.member_id},
            {"display_name", member.display_name},
            {"client_label", ""},
            {"last_seen_seconds_ago", last_seen_seconds_ago},
            {"is_host", member.is_host},
        });
    }
    return {
        {"room_code", room.room_code},
        {"session_name", room.session_name},
        {"host_name", room.host_name},
        {"session_phase", room.session_phase},
        {"authority_mode", room.authority_mode},
        {"net_protocol", room.net_protocol},
        {"realtime_endpoint", room.realtime_endpoint},
        {"connection_candidates", room.connection_candidates.is_array()
                                      ? room.connection_candidates
                                      : nlohmann::json::array()},
        {"game_version", room.game_version},
        {"mod_hash", room.mod_hash},
        {"required_mod_ids", room.required_mod_ids},
        {"content_revision", room.content_revision},
        {"allow_live_mod_reload", room.allow_live_mod_reload},
        {"privacy", room.privacy},
        {"max_players", room.max_players},
        {"current_players", static_cast<int>(room.members.size())},
        {"in_game", room.in_game},
        {"game_config", room.game_config.is_object() ? room.game_config : nlohmann::json::object()},
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

std::uint64_t json_u64(const nlohmann::json& body, const char* key, std::uint64_t fallback = 0) {
    auto it = body.find(key);
    if (it != body.end() && it->is_number_unsigned())
        return it->get<std::uint64_t>();
    return fallback;
}

std::vector<std::string> json_string_array(const nlohmann::json& body, const char* key) {
    std::vector<std::string> out;
    auto it = body.find(key);
    if (it == body.end() || !it->is_array())
        return out;
    for (const auto& entry : *it) {
        if (entry.is_string())
            out.push_back(entry.get<std::string>());
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

nlohmann::json json_array_or_empty(const nlohmann::json& body, const char* key) {
    auto it = body.find(key);
    if (it != body.end() && it->is_array())
        return *it;
    return nlohmann::json::array();
}

void ensure_nat_punch_candidate(RoomRecord& room) {
    if (!room_is_public(room))
        return;
    if (!room.connection_candidates.is_array())
        room.connection_candidates = nlohmann::json::array();
    for (const auto& candidate : room.connection_candidates) {
        if (candidate.is_object() && candidate.value("kind", "") == "nat_punch")
            return;
    }
    room.connection_candidates.push_back({
        {"kind", "nat_punch"},
        {"priority", 200},
        {"label", "NAT traversal"},
    });
}

bool body_member_access(RoomRecord& room, const nlohmann::json& body, RoomMember*& member_out,
                        httplib::Response& res) {
    RoomMember* member = find_member(room, json_string(body, "member_id"));
    if (!member) {
        res.status = 404;
        res.set_content("member not found", "text/plain");
        return false;
    }
    member->last_seen = Clock::now();
    member_out = member;
    return true;
}

const char* dashboard_html() {
    return R"(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>gubsy-roomd</title>
  <style>
    :root {
      color-scheme: dark;
      font-family: ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #171914;
      color: #f5f1df;
    }
    body {
      margin: 0;
      min-height: 100vh;
      background:
        radial-gradient(circle at 20% 0%, rgba(117, 158, 99, 0.22), transparent 32rem),
        linear-gradient(180deg, #202318 0%, #11130f 100%);
    }
    main {
      width: min(960px, calc(100% - 32px));
      margin: 0 auto;
      padding: 32px 0;
    }
    header {
      display: flex;
      align-items: end;
      justify-content: space-between;
      gap: 16px;
      margin-bottom: 24px;
    }
    h1 {
      margin: 0;
      font-size: clamp(28px, 5vw, 48px);
      line-height: 1;
      letter-spacing: 0;
    }
    .status {
      color: #c9c0a1;
      font-size: 14px;
      white-space: nowrap;
    }
    .rooms {
      display: grid;
      gap: 12px;
    }
    .room, .empty, .error {
      border: 1px solid rgba(245, 241, 223, 0.16);
      border-radius: 8px;
      background: rgba(17, 19, 15, 0.72);
      padding: 16px;
      box-shadow: 0 12px 32px rgba(0, 0, 0, 0.24);
    }
    .room {
      display: grid;
      grid-template-columns: minmax(0, 1fr) auto;
      gap: 12px;
      align-items: center;
    }
    .title {
      min-width: 0;
      font-weight: 700;
      font-size: 18px;
      overflow-wrap: anywhere;
    }
    .meta {
      margin-top: 6px;
      color: #c9c0a1;
      font-size: 14px;
      overflow-wrap: anywhere;
    }
    .code {
      border-radius: 6px;
      background: #d8b85c;
      color: #18150b;
      padding: 8px 10px;
      font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
      font-weight: 800;
      letter-spacing: 0;
    }
    .error {
      border-color: rgba(238, 100, 88, 0.45);
      color: #ffb6ad;
    }
    @media (max-width: 560px) {
      header, .room {
        align-items: start;
        grid-template-columns: 1fr;
      }
      header {
        display: grid;
      }
      .status {
        white-space: normal;
      }
      .code {
        width: max-content;
      }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <h1>gubsy-roomd</h1>
      <div id="status" class="status">Loading rooms...</div>
    </header>
    <section id="rooms" class="rooms" aria-live="polite"></section>
  </main>
  <script>
    const roomsEl = document.getElementById('rooms');
    const statusEl = document.getElementById('status');

    function text(value, fallback) {
      return value === undefined || value === null || value === '' ? fallback : String(value);
    }

    function roomCard(room) {
      const el = document.createElement('article');
      el.className = 'room';

      const body = document.createElement('div');
      const title = document.createElement('div');
      title.className = 'title';
      title.textContent = text(room.session_name, 'Online Lobby');
      const meta = document.createElement('div');
      meta.className = 'meta';
      const players = `${text(room.current_players, 0)}/${text(room.max_players, '?')} players`;
      const host = `hosted by ${text(room.host_name, 'Host')}`;
      const phase = text(room.session_phase, 'lobby');
      const authority = text(room.authority_mode, 'player_host');
      const candidates = Array.isArray(room.connection_candidates) ? room.connection_candidates.length : 0;
      meta.textContent = `${players} - ${host} - ${phase} - ${authority} - ${candidates} candidates`;
      body.append(title, meta);

      const code = document.createElement('div');
      code.className = 'code';
      code.textContent = text(room.room_code, '------');
      el.append(body, code);
      return el;
    }

    async function refresh() {
      try {
        const res = await fetch('/rooms', { cache: 'no-store' });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        const rooms = Array.isArray(data.rooms) ? data.rooms : [];
        roomsEl.replaceChildren(...(rooms.length
          ? rooms.map(roomCard)
          : [Object.assign(document.createElement('div'), {
              className: 'empty',
              textContent: 'No public games are active.'
            })]));
        statusEl.textContent = `${rooms.length} active public game${rooms.length === 1 ? '' : 's'} - updates every 2s`;
      } catch (err) {
        const el = document.createElement('div');
        el.className = 'error';
        el.textContent = `Failed to load rooms: ${err.message}`;
        roomsEl.replaceChildren(el);
        statusEl.textContent = 'Room service error';
      }
    }

    refresh();
    setInterval(refresh, 2000);
  </script>
</body>
</html>)";
}

class RendezvousUdpServer {
public:
    ~RendezvousUdpServer() { stop(); }

    bool start(const std::string& bind_host, int port, std::string& err) {
#if defined(_WIN32)
        WSADATA data{};
        const int wsa_rc = WSAStartup(MAKEWORD(2, 2), &data);
        if (wsa_rc != 0) {
            err = "WSAStartup failed";
            return false;
        }
#endif
        port_ = port;
        socket_ = open_bound_socket(bind_host, port, err);
        if (socket_ == kInvalidSocket)
            return false;
        running_.store(true);
        thread_ = std::thread([this]() { run(); });
        return true;
    }

    void stop() {
        running_.store(false);
        close_socket(socket_);
        socket_ = kInvalidSocket;
        if (thread_.joinable())
            thread_.join();
#if defined(_WIN32)
        WSACleanup();
#endif
    }

    int port() const { return port_; }

    void send_packet(const sockaddr_storage& to, socklen_t to_len, realnet::Packet packet,
                     const std::string& key) {
        if (socket_ == kInvalidSocket)
            return;
        if (packet.ts_ms == 0)
            packet.ts_ms = realnet::unix_time_ms();
        realnet::sign_packet(packet, key);
        const std::string bytes = realnet::encode_packet(packet);
        sendto(socket_, bytes.data(), static_cast<int>(bytes.size()), 0,
               reinterpret_cast<const sockaddr*>(&to), to_len);
    }

private:
    SocketHandle open_bound_socket(const std::string& bind_host, int port, std::string& err) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;
        addrinfo* result = nullptr;
        const std::string port_text = std::to_string(port);
        const char* host_arg = bind_host.empty() || bind_host == "0.0.0.0" ? nullptr : bind_host.c_str();
        const int gai = getaddrinfo(host_arg, port_text.c_str(), &hints, &result);
        if (gai != 0 || !result) {
            err = "could not resolve UDP bind endpoint";
            return kInvalidSocket;
        }

        SocketHandle out = kInvalidSocket;
        for (addrinfo* it = result; it; it = it->ai_next) {
            out = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
            if (out == kInvalidSocket)
                continue;
            if (bind(out, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0)
                break;
            close_socket(out);
            out = kInvalidSocket;
        }
        freeaddrinfo(result);
        if (out == kInvalidSocket)
            err = "could not bind UDP rendezvous socket";
        return out;
    }

    void run() {
        std::array<char, realnet::kMaxRendezvousPacketBytes + 1> buffer{};
        while (running_.load()) {
            sockaddr_storage from{};
            socklen_t from_len = sizeof(from);
            const int received = recvfrom(socket_, buffer.data(),
                                          static_cast<int>(realnet::kMaxRendezvousPacketBytes), 0,
                                          reinterpret_cast<sockaddr*>(&from), &from_len);
            if (received <= 0)
                continue;
            handle_datagram(std::string(buffer.data(), static_cast<std::size_t>(received)), from,
                            from_len);
        }
    }

    void maybe_send_endpoint_hints(RoomRecord& room, JoinAttempt& attempt) {
        if (!room.has_host_rendezvous_endpoint || !attempt.has_joiner_endpoint)
            return;

        realnet::Packet host_hint;
        host_hint.kind = realnet::PacketKind::EndpointHint;
        host_hint.room_code = room.room_code;
        host_hint.join_attempt_id = attempt.attempt_id;
        host_hint.role = "host";
        host_hint.peer_endpoint =
            sockaddr_to_realnet_endpoint(attempt.joiner_sockaddr, attempt.joiner_sockaddr_len);
        host_hint.punch_secret = attempt.punch_secret;
        send_packet(room.host_sockaddr, room.host_sockaddr_len, host_hint, room.host_secret);

        realnet::Packet joiner_hint;
        joiner_hint.kind = realnet::PacketKind::EndpointHint;
        joiner_hint.room_code = room.room_code;
        joiner_hint.join_attempt_id = attempt.attempt_id;
        joiner_hint.role = "joiner";
        joiner_hint.peer_endpoint =
            sockaddr_to_realnet_endpoint(room.host_sockaddr, room.host_sockaddr_len);
        send_packet(attempt.joiner_sockaddr, attempt.joiner_sockaddr_len, joiner_hint,
                    attempt.punch_secret);

        log_event("punch_endpoint_hint",
                  {{"room_code", room.room_code},
                   {"join_attempt_id", attempt.attempt_id},
                   {"host_endpoint", room.host_observed_endpoint},
                   {"joiner_endpoint", attempt.joiner_observed_endpoint}});
    }

    void handle_datagram(const std::string& bytes, const sockaddr_storage& from, socklen_t from_len) {
        const auto now = Clock::now();
        const std::string source = sockaddr_to_endpoint(from, from_len);
        if (!ip_limiter_.allow(source, now)) {
            log_event("punch_rate_limit", {{"scope", "ip"}, {"source", source}});
            return;
        }

        realnet::Packet packet;
        std::string err;
        if (!realnet::decode_packet(bytes, packet, err)) {
            log_event("punch_packet_reject", {{"source", source}, {"reason", err}});
            return;
        }
        if (!room_limiter_.allow(packet.room_code, now)) {
            log_event("punch_rate_limit", {{"scope", "room"}, {"room_code", packet.room_code}});
            return;
        }

        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();
        auto room_it = g_registry.rooms.find(packet.room_code);
        if (room_it == g_registry.rooms.end()) {
            log_event("punch_packet_reject",
                      {{"source", source}, {"room_code", packet.room_code}, {"reason", "room"}});
            return;
        }
        RoomRecord& room = room_it->second;

        if (packet.kind == realnet::PacketKind::HostHello) {
            if (!realnet::verify_packet(packet, room.host_secret)) {
                log_event("punch_packet_reject", {{"source", source},
                                                   {"room_code", room.room_code},
                                                   {"reason", "host_mac"}});
                return;
            }
            room.host_sockaddr = from;
            room.host_sockaddr_len = from_len;
            room.host_observed_endpoint = source;
            room.has_host_rendezvous_endpoint = true;
            room.host_rendezvous_seen_at = now;
            log_event("punch_host_hello",
                      {{"room_code", room.room_code}, {"observed_endpoint", source}});
            for (auto& [_, attempt] : room.join_attempts)
                maybe_send_endpoint_hints(room, attempt);
            return;
        }

        if (packet.kind == realnet::PacketKind::JoinerHello) {
            JoinAttempt* attempt = find_join_attempt_by_id(room, packet.join_attempt_id);
            if (!attempt) {
                log_event("punch_packet_reject", {{"source", source},
                                                   {"room_code", room.room_code},
                                                   {"join_attempt_id", packet.join_attempt_id},
                                                   {"reason", "attempt"}});
                return;
            }
            if (!realnet::verify_packet(packet, attempt->punch_secret)) {
                log_event("punch_packet_reject", {{"source", source},
                                                   {"room_code", room.room_code},
                                                   {"join_attempt_id", attempt->attempt_id},
                                                   {"reason", "joiner_mac"}});
                return;
            }
            attempt->joiner_sockaddr = from;
            attempt->joiner_sockaddr_len = from_len;
            attempt->joiner_observed_endpoint = source;
            attempt->has_joiner_endpoint = true;
            attempt->joiner_seen_at = now;
            log_event("punch_joiner_hello", {{"room_code", room.room_code},
                                             {"join_attempt_id", attempt->attempt_id},
                                             {"observed_endpoint", source}});
            maybe_send_endpoint_hints(room, *attempt);
            return;
        }

        if (packet.kind == realnet::PacketKind::PunchResult) {
            JoinAttempt* attempt = find_join_attempt_by_id(room, packet.join_attempt_id);
            const std::string key = attempt ? attempt->punch_secret : room.host_secret;
            if (!realnet::verify_packet(packet, key)) {
                log_event("punch_packet_reject", {{"source", source},
                                                   {"room_code", room.room_code},
                                                   {"join_attempt_id", packet.join_attempt_id},
                                                   {"reason", "result_mac"}});
                return;
            }
            log_event("punch_probe_result", {{"room_code", room.room_code},
                                             {"join_attempt_id", packet.join_attempt_id},
                                             {"result", packet.result},
                                             {"source", source}});
            return;
        }

        log_event("punch_packet_reject", {{"source", source},
                                           {"room_code", room.room_code},
                                           {"reason", "unexpected_kind"},
                                           {"kind", realnet::packet_kind_name(packet.kind)}});
    }

    SocketHandle socket_{kInvalidSocket};
    std::atomic<bool> running_{false};
    std::thread thread_;
    int port_{0};
    realnet::TokenBucketRateLimiter ip_limiter_{{100.0, 200.0}};
    realnet::TokenBucketRateLimiter room_limiter_{{200.0, 400.0}};
};

} // namespace

int main(int argc, char** argv) {
    std::string bind_host = "127.0.0.1";
    int port = kDefaultPort;
    int rendezvous_port = 0;
    bool rendezvous_enabled = true;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--port=", 0) == 0) {
            try {
                port = std::stoi(arg.substr(7));
            } catch (...) {
                port = kDefaultPort;
            }
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                port = std::stoi(argv[++i]);
            } catch (...) {
                port = kDefaultPort;
            }
        } else if (arg.rfind("--host=", 0) == 0) {
            bind_host = arg.substr(7);
        } else if (arg == "--host" && i + 1 < argc) {
            bind_host = argv[++i];
        } else if (arg.rfind("--rendezvous-port=", 0) == 0) {
            try {
                rendezvous_port = std::stoi(arg.substr(18));
            } catch (...) {
                rendezvous_port = 0;
            }
        } else if (arg == "--rendezvous-port" && i + 1 < argc) {
            try {
                rendezvous_port = std::stoi(argv[++i]);
            } catch (...) {
                rendezvous_port = 0;
            }
        } else if (arg == "--no-rendezvous") {
            rendezvous_enabled = false;
        } else if (arg == "--help") {
            std::cout << "Usage: gubsy-roomd [--host=<bind-host>] [--port=<port>]\n"
                         "                    [--rendezvous-port=<udp-port>] [--no-rendezvous]\n";
            return 0;
        }
    }
    if (bind_host.empty())
        bind_host = "127.0.0.1";
    if (port <= 0 || port > 65535)
        port = kDefaultPort;
    if (rendezvous_port <= 0 || rendezvous_port > 65535)
        rendezvous_port = port + kDefaultRendezvousPortOffset;

    httplib::Server server;
    RendezvousUdpServer rendezvous;
    if (rendezvous_enabled) {
        std::string udp_err;
        if (!rendezvous.start(bind_host, rendezvous_port, udp_err)) {
            log_event("punch_rendezvous_start_failed",
                      {{"host", bind_host}, {"port", rendezvous_port}, {"error", udp_err}});
            rendezvous_enabled = false;
        }
    }

    server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(dashboard_html(), "text/html; charset=utf-8");
    });

    server.Get("/health", [&](const httplib::Request&, httplib::Response& res) {
        nlohmann::json capabilities = {
            {"ok", true},
            {"realnet",
             {{"rendezvous_udp",
               {{"enabled", rendezvous_enabled},
                {"host", bind_host},
                {"port", rendezvous_enabled ? rendezvous.port() : 0},
                {"protocol", "gubsy-rendezvous-v1"}}}}},
        };
        res.set_content(capabilities.dump(), "application/json");
    });

    server.Get("/debug/realnet", [](const httplib::Request& req, httplib::Response& res) {
        if (req.remote_addr != "127.0.0.1" && req.remote_addr != "::1") {
            res.status = 403;
            res.set_content("debug endpoints are localhost-only", "text/plain");
            return;
        }
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();
        nlohmann::json rooms = nlohmann::json::array();
        int attempts = 0;
        for (const auto& [_, room] : g_registry.rooms) {
            attempts += static_cast<int>(room.join_attempts.size());
            rooms.push_back({
                {"room_code", room.room_code},
                {"has_host_endpoint", room.has_host_rendezvous_endpoint},
                {"host_observed_endpoint", room.host_observed_endpoint},
                {"join_attempts", static_cast<int>(room.join_attempts.size())},
            });
        }
        res.set_content(nlohmann::json{{"rooms", rooms}, {"join_attempts", attempts}}.dump(),
                        "application/json");
    });

    server.Get(R"(/debug/realnet/rooms/([A-Z0-9]+))",
               [](const httplib::Request& req, httplib::Response& res) {
                   if (req.remote_addr != "127.0.0.1" && req.remote_addr != "::1") {
                       res.status = 403;
                       res.set_content("debug endpoints are localhost-only", "text/plain");
                       return;
                   }
                   std::lock_guard<std::mutex> lock(g_registry.mutex);
                   g_registry.cleanup_expired_locked();
                   auto it = g_registry.rooms.find(req.matches[1].str());
                   if (it == g_registry.rooms.end()) {
                       res.status = 404;
                       res.set_content("room not found", "text/plain");
                       return;
                   }
                   const RoomRecord& room = it->second;
                   nlohmann::json attempts = nlohmann::json::array();
                   for (const auto& [_, attempt] : room.join_attempts) {
                       attempts.push_back({
                           {"join_attempt_id", attempt.attempt_id},
                           {"display_name", attempt.display_name},
                           {"has_joiner_endpoint", attempt.has_joiner_endpoint},
                           {"joiner_observed_endpoint", attempt.joiner_observed_endpoint},
                       });
                   }
                   res.set_content(nlohmann::json{{"room_code", room.room_code},
                                                  {"has_host_endpoint",
                                                   room.has_host_rendezvous_endpoint},
                                                  {"host_observed_endpoint",
                                                   room.host_observed_endpoint},
                                                  {"join_attempts", attempts}}
                                       .dump(),
                                   "application/json");
               });

    server.Get(R"(/debug/realnet/attempts/([A-Z0-9]+))",
               [](const httplib::Request& req, httplib::Response& res) {
                   if (req.remote_addr != "127.0.0.1" && req.remote_addr != "::1") {
                       res.status = 403;
                       res.set_content("debug endpoints are localhost-only", "text/plain");
                       return;
                   }
                   std::lock_guard<std::mutex> lock(g_registry.mutex);
                   g_registry.cleanup_expired_locked();
                   for (const auto& [_, room] : g_registry.rooms) {
                       for (const auto& [__, attempt] : room.join_attempts) {
                           if (attempt.attempt_id != req.matches[1].str())
                               continue;
                           res.set_content(nlohmann::json{{"room_code", room.room_code},
                                                          {"join_attempt_id",
                                                           attempt.attempt_id},
                                                          {"has_host_endpoint",
                                                           room.has_host_rendezvous_endpoint},
                                                          {"host_observed_endpoint",
                                                           room.host_observed_endpoint},
                                                          {"has_joiner_endpoint",
                                                           attempt.has_joiner_endpoint},
                                                          {"joiner_observed_endpoint",
                                                           attempt.joiner_observed_endpoint}}
                                               .dump(),
                                           "application/json");
                           return;
                       }
                   }
                   res.status = 404;
                   res.set_content("attempt not found", "text/plain");
               });

    server.Get("/rooms", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.cleanup_expired_locked();
        nlohmann::json rooms = nlohmann::json::array();
        int public_rooms = 0;
        int hidden_rooms = 0;
        for (const auto& [_, room] : g_registry.rooms) {
            if (!room_is_public(room)) {
                ++hidden_rooms;
                continue;
            }
            ++public_rooms;
            rooms.push_back(room_to_json(room));
        }
        log_event("room_list", {{"total_rooms", static_cast<int>(g_registry.rooms.size())},
                                {"public_rooms", public_rooms},
                                {"hidden_rooms", hidden_rooms}});
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
        res.set_content(nlohmann::json{{"room", room_to_json(it->second)}}.dump(),
                        "application/json");
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
        room.session_phase = json_string(body, "session_phase",
                                         json_bool(body, "in_game", false) ? "in_game" : "lobby");
        room.authority_mode = json_string(body, "authority_mode", "player_host");
        room.net_protocol = json_string(body, "net_protocol", "gubsy-sync-1");
        room.realtime_endpoint = json_string(body, "realtime_endpoint");
        room.connection_candidates = json_array_or_empty(body, "connection_candidates");
        room.game_version = json_string(body, "game_version");
        room.mod_hash = json_string(body, "mod_hash");
        room.required_mod_ids = json_string_array(body, "required_mod_ids");
        room.content_revision = std::max<std::uint64_t>(1, json_u64(body, "content_revision", 1));
        room.allow_live_mod_reload = json_bool(body, "allow_live_mod_reload", true);
        room.privacy = json_int(body, "privacy", 0);
        room.max_players = std::max(1, json_int(body, "max_players", 4));
        room.in_game = room.session_phase == "in_game" || json_bool(body, "in_game", false);
        ensure_nat_punch_candidate(room);
        auto game_config_it = body.find("game_config");
        if (game_config_it != body.end() && game_config_it->is_object())
            room.game_config = *game_config_it;

        RoomMember host;
        host.member_id = g_registry.random_token(kMemberIdLen);
        host.display_name = room.host_name;
        host.is_host = true;
        room.members.push_back(std::move(host));

        const std::string member_id = room.members.front().member_id;
        const std::string room_code = room.room_code;
        const int privacy = room.privacy;
        const std::string endpoint = room.realtime_endpoint;
        g_registry.rooms.emplace(room.room_code, room);

        log_event("room_create", {
                                     {"room_code", room_code},
                                     {"privacy", privacy},
                                     {"realtime_endpoint", endpoint},
                                     {"authority_mode", g_registry.rooms.at(room_code).authority_mode},
                                     {"connection_candidates",
                                      static_cast<int>(g_registry.rooms.at(room_code)
                                                           .connection_candidates.size())},
                                 });
        res.set_content(
            nlohmann::json{
                {"room_code", room_code},
                {"host_secret", g_registry.rooms.at(room_code).host_secret},
                {"member_id", member_id},
            }
                .dump(),
            "application/json");
    });

    server.Post(
        R"(/rooms/([A-Z0-9]+)/join_attempt)",
        [](const httplib::Request& req, httplib::Response& res) {
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

            JoinAttempt attempt;
            attempt.attempt_id = g_registry.random_token(kJoinAttemptIdLen);
            attempt.token = g_registry.random_token(kSecretLen);
            attempt.punch_secret = g_registry.random_token(kSecretLen);
            attempt.display_name = json_string(body, "display_name", "Guest");
            const std::string attempt_id = attempt.attempt_id;
            const std::string token = attempt.token;
            const std::string punch_secret = attempt.punch_secret;
            room.join_attempts.emplace(token, std::move(attempt));
            room.updated_at = Clock::now();

            log_event("room_join_attempt", {
                                               {"room_code", room.room_code},
                                               {"join_attempt_id", attempt_id},
                                           });
            log_event("punch_attempt_create",
                      {{"room_code", room.room_code}, {"join_attempt_id", attempt_id}});
            res.set_content(
                nlohmann::json{
                    {"join_attempt_id", attempt_id},
                    {"join_token", token},
                    {"punch_secret", punch_secret},
                    {"room", room_to_json(room)},
                }
                    .dump(),
                "application/json");
        });

    server.Post(
        R"(/rooms/([A-Z0-9]+)/join)", [](const httplib::Request& req, httplib::Response& res) {
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

            const std::string join_token = json_string(body, "join_token");
            if (!join_token.empty()) {
                auto attempt_it = room.join_attempts.find(join_token);
                if (attempt_it == room.join_attempts.end()) {
                    res.status = 403;
                    res.set_content("join token rejected", "text/plain");
                    return;
                }
                room.join_attempts.erase(attempt_it);
            }

            RoomMember member;
            member.member_id = g_registry.random_token(kMemberIdLen);
            member.display_name = json_string(body, "display_name", "Guest");
            room.members.push_back(member);
            room.updated_at = Clock::now();

            log_event("room_join", {
                                       {"room_code", room.room_code},
                                       {"member_id", member.member_id},
                                       {"current_players", static_cast<int>(room.members.size())},
                                   });
            res.set_content(
                nlohmann::json{
                    {"member_id", member.member_id},
                    {"room", room_to_json(room)},
                }
                    .dump(),
                "application/json");
        });

    server.Post(R"(/rooms/([A-Z0-9]+)/heartbeat)", [](const httplib::Request& req,
                                                      httplib::Response& res) {
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
        RoomMember* member = nullptr;
        if (!body_member_access(room, body, member, res))
            return;

        member->display_name = json_string(body, "display_name", member->display_name.c_str());

        const std::string host_secret = json_string(body, "host_secret");
        auto room_it = body.find("room");
        if (room_it != body.end() && room_it->is_object()) {
            if (!member->is_host || host_secret != room.host_secret) {
                res.status = 403;
                res.set_content("host secret mismatch", "text/plain");
                return;
            }
            room.session_name = json_string(*room_it, "session_name", room.session_name.c_str());
            room.host_name = json_string(*room_it, "host_name", room.host_name.c_str());
            room.session_phase = json_string(*room_it, "session_phase",
                                             room.in_game ? "in_game" : room.session_phase.c_str());
            room.authority_mode =
                json_string(*room_it, "authority_mode", room.authority_mode.c_str());
            room.net_protocol = json_string(*room_it, "net_protocol", room.net_protocol.c_str());
            room.realtime_endpoint =
                json_string(*room_it, "realtime_endpoint", room.realtime_endpoint.c_str());
            if (room_it->contains("connection_candidates"))
                room.connection_candidates = json_array_or_empty(*room_it, "connection_candidates");
            room.game_version = json_string(*room_it, "game_version", room.game_version.c_str());
            room.mod_hash = json_string(*room_it, "mod_hash", room.mod_hash.c_str());
            room.required_mod_ids = json_string_array(*room_it, "required_mod_ids");
            room.content_revision = std::max<std::uint64_t>(
                1, json_u64(*room_it, "content_revision", room.content_revision));
            room.allow_live_mod_reload =
                json_bool(*room_it, "allow_live_mod_reload", room.allow_live_mod_reload);
            room.privacy = json_int(*room_it, "privacy", room.privacy);
            room.max_players = std::max(1, json_int(*room_it, "max_players", room.max_players));
            room.in_game =
                room.session_phase == "in_game" || json_bool(*room_it, "in_game", room.in_game);
            ensure_nat_punch_candidate(room);
            auto game_config_it = room_it->find("game_config");
            if (game_config_it != room_it->end() && game_config_it->is_object())
                room.game_config = *game_config_it;
            member->display_name = room.host_name;
            log_event("room_update", {
                                         {"room_code", room.room_code},
                                         {"privacy", room.privacy},
                                         {"session_phase", room.session_phase},
                                         {"current_players", static_cast<int>(room.members.size())},
                                     });
        }

        room.updated_at = Clock::now();
        res.set_content(nlohmann::json{{"ok", true}}.dump(), "application/json");
    });

    server.Post(R"(/rooms/([A-Z0-9]+)/leave)", [](const httplib::Request& req,
                                                  httplib::Response& res) {
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

        auto member_it =
            std::find_if(room.members.begin(), room.members.end(),
                         [&](const RoomMember& member) { return member.member_id == member_id; });
        if (member_it != room.members.end()) {
            const bool is_host = member_it->is_host;
            room.members.erase(member_it);
            log_event("room_leave", {
                                        {"room_code", room.room_code},
                                        {"member_id", member_id},
                                        {"current_players", static_cast<int>(room.members.size())},
                                    });
            if (is_host || host_secret == room.host_secret) {
                log_event("room_delete", {{"room_code", room.room_code}});
                g_registry.rooms.erase(it);
            }
        }
        res.set_content(R"({"ok":true})", "application/json");
    });

    server.Post(R"(/rooms/([A-Z0-9]+)/remove_member)", [](const httplib::Request& req,
                                                          httplib::Response& res) {
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
        if (json_string(body, "host_secret") != room.host_secret) {
            res.status = 403;
            res.set_content("host secret mismatch", "text/plain");
            return;
        }

        const std::string member_id = json_string(body, "member_id");
        auto member_it =
            std::find_if(room.members.begin(), room.members.end(),
                         [&](const RoomMember& member) { return member.member_id == member_id; });
        if (member_it == room.members.end()) {
            res.status = 404;
            res.set_content("member not found", "text/plain");
            return;
        }
        if (member_it->is_host) {
            res.status = 409;
            res.set_content("cannot remove host", "text/plain");
            return;
        }

        room.members.erase(member_it);
        room.updated_at = Clock::now();
        log_event("room_remove_member",
                  {
                      {"room_code", room.room_code},
                      {"member_id", member_id},
                      {"current_players", static_cast<int>(room.members.size())},
                  });
        res.set_content(R"({"ok":true})", "application/json");
    });

    log_event("roomd_start", {{"host", bind_host}, {"port", port}});
    server.listen(bind_host, port);
    return 0;
}
