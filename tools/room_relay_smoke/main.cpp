#include "gubsy/realnet/relay.hpp"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
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

namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

struct HttpEndpoint {
    std::string host{"127.0.0.1"};
    int port{8788};
};

struct UdpSocket {
    SocketHandle handle{kInvalidSocket};

    ~UdpSocket() { close(); }

    void close() {
        if (handle == kInvalidSocket)
            return;
#if defined(_WIN32)
        closesocket(handle);
#else
        ::close(handle);
#endif
        handle = kInvalidSocket;
    }
};

int fail(const std::string& message) {
    std::cerr << "room_relay_smoke: " << message << '\n';
    return 1;
}

std::optional<HttpEndpoint> parse_http_endpoint(std::string url) {
    constexpr const char* prefix = "http://";
    if (url.rfind(prefix, 0) != 0)
        return std::nullopt;
    url = url.substr(7);
    const auto slash = url.find('/');
    if (slash != std::string::npos)
        url = url.substr(0, slash);
    HttpEndpoint endpoint;
    const auto colon = url.rfind(':');
    if (colon == std::string::npos) {
        endpoint.host = url;
    } else {
        endpoint.host = url.substr(0, colon);
        endpoint.port = std::stoi(url.substr(colon + 1));
    }
    if (endpoint.host.empty() || endpoint.port <= 0 || endpoint.port > 65535)
        return std::nullopt;
    return endpoint;
}

std::optional<nlohmann::json> post_json(const HttpEndpoint& endpoint,
                                        const std::string& path,
                                        const nlohmann::json& body,
                                        std::string& err) {
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);
    auto res = client.Post(path.c_str(), body.dump(), "application/json");
    if (!res) {
        err = "http post failed";
        return std::nullopt;
    }
    if (res->status < 200 || res->status >= 300) {
        err = "http post returned " + std::to_string(res->status) + ": " + res->body;
        return std::nullopt;
    }
    return nlohmann::json::parse(res->body);
}

std::optional<nlohmann::json> get_json(const HttpEndpoint& endpoint,
                                       const std::string& path,
                                       std::string& err) {
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);
    auto res = client.Get(path.c_str());
    if (!res) {
        err = "http get failed";
        return std::nullopt;
    }
    if (res->status < 200 || res->status >= 300) {
        err = "http get returned " + std::to_string(res->status) + ": " + res->body;
        return std::nullopt;
    }
    return nlohmann::json::parse(res->body);
}

UdpSocket open_udp_socket() {
    UdpSocket socket;
    socket.handle = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket.handle == kInvalidSocket)
        return socket;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(socket.handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        socket.close();
        return socket;
    }

#if defined(_WIN32)
    u_long non_blocking = 1;
    ioctlsocket(socket.handle, FIONBIO, &non_blocking);
#else
    const int flags = fcntl(socket.handle, F_GETFL, 0);
    fcntl(socket.handle, F_SETFL, flags | O_NONBLOCK);
#endif
    return socket;
}

bool send_relay_packet(SocketHandle socket,
                       const std::string& host,
                       int port,
                       realnet::RelayPacket packet,
                       const std::string& key) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    const std::string port_text = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0 || !result)
        return false;
    realnet::sign_relay_packet(packet, key);
    const std::string bytes = realnet::encode_relay_packet(packet);
    const int sent = sendto(socket, bytes.data(), static_cast<int>(bytes.size()), 0,
                            result->ai_addr, static_cast<int>(result->ai_addrlen));
    freeaddrinfo(result);
    return sent == static_cast<int>(bytes.size());
}

bool receive_relay_packet(SocketHandle socket,
                          const std::string& key,
                          realnet::RelayPacket& out) {
    char buffer[realnet::kMaxRelayPacketBytes + 1]{};
    const int received = recvfrom(socket, buffer, realnet::kMaxRelayPacketBytes, 0, nullptr,
                                  nullptr);
    if (received <= 0)
        return false;
    std::string err;
    if (!realnet::decode_relay_packet(std::string(buffer, static_cast<std::size_t>(received)),
                                      out,
                                      err)) {
        return false;
    }
    return realnet::verify_relay_packet(out, key);
}

bool wait_for_packet(SocketHandle socket,
                     const std::string& key,
                     realnet::RelayPacketKind kind,
                     realnet::RelayPacket& out) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        if (receive_relay_packet(socket, key, out) && out.kind == kind)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

std::uint64_t relay_counter(const nlohmann::json& debug, const std::string& name) {
    const auto counters = debug.value("relay_counters", nlohmann::json::object());
    return counters.value(name, std::uint64_t{0});
}

bool wait_for_counter_at_least(const HttpEndpoint& endpoint,
                               const std::string& name,
                               std::uint64_t value,
                               std::string& err) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto debug = get_json(endpoint, "/debug/realnet", err);
        if (debug && relay_counter(*debug, name) >= value)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

bool wait_for_room_allocations(const HttpEndpoint& endpoint,
                               const std::string& room_code,
                               std::size_t count,
                               std::string& err) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto debug = get_json(endpoint, "/debug/realnet/rooms/" + room_code, err);
        if (debug &&
            debug->value("relay_allocations", nlohmann::json::array()).size() == count) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        return fail("WSAStartup failed");
#endif

    const std::string server_url = argc > 1 ? argv[1] : "http://127.0.0.1:8788";
    const auto endpoint = parse_http_endpoint(server_url);
    if (!endpoint)
        return fail("bad server url");

    std::string err;
    const auto health = get_json(*endpoint, "/health", err);
    if (!health)
        return fail(err);
    const auto relay_health = (*health)["realnet"]["relay_udp"];
    const int relay_port = relay_health.value("port", 0);
    if (!relay_health.value("enabled", false) || relay_port <= 0)
        return fail("relay UDP not advertised");

    const auto created = post_json(
        *endpoint,
        "/rooms/create",
        {{"session_name", "Relay Smoke"},
         {"host_name", "Host"},
         {"privacy", 1},
         {"max_players", 4},
         {"realtime_endpoint", "127.0.0.1:35355"},
         {"connection_candidates", nlohmann::json::array()}},
        err);
    if (!created)
        return fail(err);
    const std::string room_code = created->value("room_code", "");
    const std::string host_secret = created->value("host_secret", "");
    if (room_code.empty() || host_secret.empty())
        return fail("room creation did not return credentials");

    const auto attempt = post_json(*endpoint,
                                   "/rooms/" + room_code + "/join_attempt",
                                   {{"display_name", "Joiner"}},
                                   err);
    if (!attempt)
        return fail(err);
    const std::string attempt_id = attempt->value("join_attempt_id", "");
    const std::string allocation_id = attempt->value("relay_allocation_id", "");
    const std::string relay_secret = attempt->value("relay_secret", "");
    if (attempt_id.empty() || allocation_id.empty() || relay_secret.empty())
        return fail("join attempt did not return relay credentials");

    const auto capped_attempt = post_json(*endpoint,
                                          "/rooms/" + room_code + "/join_attempt",
                                          {{"display_name", "Capped Joiner"}},
                                          err);
    if (!capped_attempt)
        return fail(err);
    if (!capped_attempt->value("relay_allocation_id", "").empty() ||
        !capped_attempt->value("relay_secret", "").empty()) {
        return fail("relay allocation cap did not suppress relay credentials");
    }
    if (!wait_for_counter_at_least(*endpoint, "allocations_rejected", 1, err))
        return fail("relay allocation rejection counter did not increment");

    UdpSocket host_socket = open_udp_socket();
    UdpSocket joiner_socket = open_udp_socket();
    UdpSocket changed_joiner_socket = open_udp_socket();
    UdpSocket bad_auth_socket = open_udp_socket();
    if (host_socket.handle == kInvalidSocket || joiner_socket.handle == kInvalidSocket ||
        changed_joiner_socket.handle == kInvalidSocket || bad_auth_socket.handle == kInvalidSocket) {
        return fail("could not open UDP sockets");
    }

    realnet::RelayPacket host_hello;
    host_hello.kind = realnet::RelayPacketKind::Hello;
    host_hello.role = realnet::RelayRole::Host;
    host_hello.seq = 1;
    host_hello.room_code = room_code;
    if (!send_relay_packet(host_socket.handle, endpoint->host, relay_port, host_hello, host_secret))
        return fail("could not send host relay hello");

    realnet::RelayPacket joiner_hello;
    joiner_hello.kind = realnet::RelayPacketKind::Hello;
    joiner_hello.role = realnet::RelayRole::Joiner;
    joiner_hello.seq = 1;
    joiner_hello.room_code = room_code;
    joiner_hello.allocation_id = allocation_id;
    joiner_hello.join_attempt_id = attempt_id;
    if (!send_relay_packet(joiner_socket.handle,
                           endpoint->host,
                           relay_port,
                           joiner_hello,
                           relay_secret)) {
        return fail("could not send joiner relay hello");
    }

    realnet::RelayPacket host_ready;
    realnet::RelayPacket joiner_ready;
    if (!wait_for_packet(host_socket.handle, host_secret, realnet::RelayPacketKind::Ready, host_ready))
        return fail("host did not receive relay ready");
    if (!wait_for_packet(joiner_socket.handle,
                         relay_secret,
                         realnet::RelayPacketKind::Ready,
                         joiner_ready)) {
        return fail("joiner did not receive relay ready");
    }
    if (host_ready.allocation_id != allocation_id || joiner_ready.allocation_id != allocation_id)
        return fail("relay ready allocation mismatch");

    realnet::RelayPacket joiner_data;
    joiner_data.kind = realnet::RelayPacketKind::Data;
    joiner_data.role = realnet::RelayRole::Joiner;
    joiner_data.room_code = room_code;
    joiner_data.allocation_id = allocation_id;
    joiner_data.join_attempt_id = attempt_id;
    joiner_data.payload = {0x01, 0x02, 0xff, 0x00};
    if (!send_relay_packet(joiner_socket.handle,
                           endpoint->host,
                           relay_port,
                           joiner_data,
                           relay_secret)) {
        return fail("could not send joiner relay data");
    }
    realnet::RelayPacket relayed_to_host;
    if (!wait_for_packet(host_socket.handle,
                         host_secret,
                         realnet::RelayPacketKind::Data,
                         relayed_to_host)) {
        return fail("host did not receive relayed data");
    }
    if (relayed_to_host.payload != joiner_data.payload)
        return fail("host received wrong relayed payload");

    realnet::RelayPacket host_data;
    host_data.kind = realnet::RelayPacketKind::Data;
    host_data.role = realnet::RelayRole::Host;
    host_data.room_code = room_code;
    host_data.allocation_id = allocation_id;
    host_data.join_attempt_id = attempt_id;
    host_data.payload = {0x7f, 0x00, 0x10};
    if (!send_relay_packet(host_socket.handle, endpoint->host, relay_port, host_data, host_secret))
        return fail("could not send host relay data");
    realnet::RelayPacket relayed_to_joiner;
    if (!wait_for_packet(joiner_socket.handle,
                         relay_secret,
                         realnet::RelayPacketKind::Data,
                         relayed_to_joiner)) {
        return fail("joiner did not receive relayed data");
    }
    if (relayed_to_joiner.payload != host_data.payload)
        return fail("joiner received wrong relayed payload");

    realnet::RelayPacket replayed_keepalive;
    replayed_keepalive.kind = realnet::RelayPacketKind::Keepalive;
    replayed_keepalive.role = realnet::RelayRole::Joiner;
    replayed_keepalive.seq = 2;
    replayed_keepalive.room_code = room_code;
    replayed_keepalive.allocation_id = allocation_id;
    replayed_keepalive.join_attempt_id = attempt_id;
    if (!send_relay_packet(joiner_socket.handle,
                           endpoint->host,
                           relay_port,
                           replayed_keepalive,
                           relay_secret)) {
        return fail("could not send relay keepalive");
    }
    if (!send_relay_packet(joiner_socket.handle,
                           endpoint->host,
                           relay_port,
                           replayed_keepalive,
                           relay_secret)) {
        return fail("could not send replayed relay keepalive");
    }
    if (!wait_for_counter_at_least(*endpoint, "replayed_control_packets", 1, err))
        return fail("relay replay counter did not increment");

    realnet::RelayPacket changed_endpoint_data;
    changed_endpoint_data.kind = realnet::RelayPacketKind::Data;
    changed_endpoint_data.role = realnet::RelayRole::Joiner;
    changed_endpoint_data.room_code = room_code;
    changed_endpoint_data.allocation_id = allocation_id;
    changed_endpoint_data.join_attempt_id = attempt_id;
    changed_endpoint_data.payload = {0x09};
    if (!send_relay_packet(changed_joiner_socket.handle,
                           endpoint->host,
                           relay_port,
                           changed_endpoint_data,
                           relay_secret)) {
        return fail("could not send changed-endpoint relay data");
    }
    if (!wait_for_counter_at_least(*endpoint, "endpoint_mismatches", 1, err))
        return fail("relay endpoint mismatch counter did not increment");

    realnet::RelayPacket bad_auth;
    bad_auth.kind = realnet::RelayPacketKind::Hello;
    bad_auth.role = realnet::RelayRole::Joiner;
    bad_auth.seq = 1;
    bad_auth.room_code = room_code;
    bad_auth.allocation_id = allocation_id;
    bad_auth.join_attempt_id = attempt_id;
    if (!send_relay_packet(bad_auth_socket.handle,
                           endpoint->host,
                           relay_port,
                           bad_auth,
                           "wrong-relay-secret") ||
        !send_relay_packet(bad_auth_socket.handle,
                           endpoint->host,
                           relay_port,
                           bad_auth,
                           "wrong-relay-secret")) {
        return fail("could not send bad-auth relay packets");
    }
    if (!wait_for_counter_at_least(*endpoint, "auth_bans", 1, err))
        return fail("relay auth ban counter did not increment");

    const auto debug = get_json(*endpoint, "/debug/realnet/rooms/" + room_code, err);
    if (!debug)
        return fail(err);
    const auto allocations = debug->value("relay_allocations", nlohmann::json::array());
    if (allocations.empty() || !allocations.front().value("ready", false))
        return fail("relay allocation debug state was not ready");

    realnet::RelayPacket close;
    close.kind = realnet::RelayPacketKind::Close;
    close.role = realnet::RelayRole::Joiner;
    close.seq = 3;
    close.room_code = room_code;
    close.allocation_id = allocation_id;
    close.join_attempt_id = attempt_id;
    if (!send_relay_packet(joiner_socket.handle, endpoint->host, relay_port, close, relay_secret))
        return fail("could not send relay close");
    if (!wait_for_room_allocations(*endpoint, room_code, 0, err))
        return fail("relay allocation did not close");
    if (!wait_for_counter_at_least(*endpoint, "allocations_closed", 1, err))
        return fail("relay close counter did not increment");

    std::cout << "room_relay_smoke: ok\n";
#if defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}
