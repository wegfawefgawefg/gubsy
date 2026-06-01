#include "../room_server/realnet_rendezvous.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

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
    std::cerr << "room_punch_smoke: " << message << '\n';
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

std::optional<nlohmann::json> post_json(const HttpEndpoint& endpoint, const std::string& path,
                                        const nlohmann::json& body, std::string& err) {
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

std::optional<nlohmann::json> get_json(const HttpEndpoint& endpoint, const std::string& path,
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

bool send_packet(SocketHandle socket, const std::string& host, int port, realnet::Packet packet,
                 const std::string& key) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* result = nullptr;
    const std::string port_text = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &result) != 0 || !result)
        return false;
    packet.ts_ms = realnet::unix_time_ms();
    realnet::sign_packet(packet, key);
    const std::string bytes = realnet::encode_packet(packet);
    const int sent = sendto(socket, bytes.data(), static_cast<int>(bytes.size()), 0,
                            result->ai_addr, static_cast<int>(result->ai_addrlen));
    freeaddrinfo(result);
    return sent == static_cast<int>(bytes.size());
}

bool receive_hint(SocketHandle socket, const std::string& key, realnet::Packet& out) {
    char buffer[realnet::kMaxRendezvousPacketBytes + 1]{};
    const int received = recvfrom(socket, buffer, realnet::kMaxRendezvousPacketBytes, 0, nullptr,
                                  nullptr);
    if (received <= 0)
        return false;
    std::string err;
    if (!realnet::decode_packet(std::string(buffer, static_cast<std::size_t>(received)), out, err))
        return false;
    return out.kind == realnet::PacketKind::EndpointHint && realnet::verify_packet(out, key) &&
           out.peer_endpoint.has_value();
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
    const int rendezvous_port =
        (*health)["realnet"]["rendezvous_udp"].value("port", 0);
    if (rendezvous_port <= 0)
        return fail("rendezvous UDP not advertised");

    const auto created = post_json(
        *endpoint,
        "/rooms/create",
        {{"session_name", "Punch Smoke"},
         {"host_name", "Host"},
         {"privacy", 1},
         {"max_players", 4},
         {"realtime_endpoint", "127.0.0.1:35355"},
         {"connection_candidates",
          nlohmann::json::array({{{"kind", "lan_direct"},
                                  {"priority", 100},
                                  {"endpoint", "127.0.0.1:35355"}},
                                 {{"kind", "nat_punch"},
                                  {"priority", 200},
                                  {"label", "NAT traversal"}}})}},
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
    const std::string punch_secret = attempt->value("punch_secret", "");
    if (attempt_id.empty() || punch_secret.empty())
        return fail("join attempt did not return punch credentials");

    UdpSocket host_socket = open_udp_socket();
    UdpSocket joiner_socket = open_udp_socket();
    if (host_socket.handle == kInvalidSocket || joiner_socket.handle == kInvalidSocket)
        return fail("could not open UDP sockets");

    realnet::Packet host_hello;
    host_hello.kind = realnet::PacketKind::HostHello;
    host_hello.room_code = room_code;
    host_hello.role = "host";
    if (!send_packet(host_socket.handle, endpoint->host, rendezvous_port, host_hello, host_secret))
        return fail("could not send host hello");

    realnet::Packet joiner_hello;
    joiner_hello.kind = realnet::PacketKind::JoinerHello;
    joiner_hello.room_code = room_code;
    joiner_hello.join_attempt_id = attempt_id;
    joiner_hello.role = "joiner";
    if (!send_packet(joiner_socket.handle,
                     endpoint->host,
                     rendezvous_port,
                     joiner_hello,
                     punch_secret))
        return fail("could not send joiner hello");

    bool got_host_hint = false;
    bool got_joiner_hint = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline && (!got_host_hint || !got_joiner_hint)) {
        realnet::Packet hint;
        if (!got_host_hint && receive_hint(host_socket.handle, host_secret, hint))
            got_host_hint = true;
        if (!got_joiner_hint && receive_hint(joiner_socket.handle, punch_secret, hint))
            got_joiner_hint = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!got_host_hint || !got_joiner_hint)
        return fail("did not receive both endpoint hints");

    const auto debug = get_json(*endpoint, "/debug/realnet/attempts/" + attempt_id, err);
    if (!debug)
        return fail(err);
    if (!debug->value("has_host_endpoint", false) || !debug->value("has_joiner_endpoint", false))
        return fail("debug endpoint does not show both observed endpoints");

    std::cout << "room_punch_smoke: ok\n";
#if defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}
