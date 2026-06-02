#include "relay_udp_server.hpp"

#include "gubsy/realnet/rendezvous.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <unistd.h>
#endif

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace room_server {
namespace {

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

std::string sockaddr_to_endpoint(const sockaddr_storage& storage, socklen_t len) {
    char host[NI_MAXHOST]{};
    char service[NI_MAXSERV]{};
    const int rc = getnameinfo(reinterpret_cast<const sockaddr*>(&storage),
                               len,
                               host,
                               sizeof(host),
                               service,
                               sizeof(service),
                               NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
        return {};
    return std::string(host) + ":" + service;
}

SocketHandle open_bound_socket(const std::string& bind_host, int port, std::string& err) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* result = nullptr;
    const std::string port_text = std::to_string(port);
    const char* host_arg =
        bind_host.empty() || bind_host == "0.0.0.0" ? nullptr : bind_host.c_str();
    const int gai = getaddrinfo(host_arg, port_text.c_str(), &hints, &result);
    if (gai != 0 || !result) {
        err = "could not resolve UDP relay bind endpoint";
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
        err = "could not bind UDP relay socket";
    return out;
}

} // namespace

struct RelayUdpServer::Impl {
    SocketHandle socket{kInvalidSocket};
    std::atomic<bool> running{false};
    std::thread thread;
    int port{0};
    realnet::RelayServiceConfig config{realnet::default_config().relay};
    realnet::TokenBucketRateLimiter ip_limiter{
        {config.ip_packet_rate_per_sec, config.ip_packet_burst}
    };
    realnet::TokenBucketRateLimiter room_limiter{
        {config.room_packet_rate_per_sec, config.room_packet_burst}
    };
    RelayDatagramHandler datagram_handler;
    RelayDropHandler drop_handler;
    RelayLogHandler log_handler;

    void count_drop(bool auth_failure = false, bool rate_limited = false) {
        if (drop_handler)
            drop_handler(auth_failure, rate_limited);
    }

    void log(const char* event, const nlohmann::json& fields) {
        if (log_handler)
            log_handler(event, fields);
    }

    void run() {
        std::array<char, realnet::kMaxRelayPacketBytes + 1> buffer{};
        while (running.load()) {
            sockaddr_storage from{};
            socklen_t from_len = sizeof(from);
            const int received = recvfrom(socket,
                                          buffer.data(),
                                          static_cast<int>(realnet::kMaxRelayPacketBytes),
                                          0,
                                          reinterpret_cast<sockaddr*>(&from),
                                          &from_len);
            if (received <= 0)
                continue;
            handle_datagram(std::string(buffer.data(), static_cast<std::size_t>(received)),
                            from,
                            from_len);
        }
    }

    void handle_datagram(const std::string& bytes,
                         const sockaddr_storage& from,
                         socklen_t from_len) {
        const auto now = RelayClock::now();
        const std::string source = sockaddr_to_endpoint(from, from_len);
        if (!ip_limiter.allow(source, now)) {
            count_drop(false, true);
            log("relay_rate_limit", {{"scope", "ip"}, {"source", source}});
            return;
        }
        if (bytes.size() > config.max_packet_bytes) {
            count_drop();
            log("relay_packet_reject",
                {{"source", source},
                 {"reason", "relay_packet_too_large"},
                 {"bytes", static_cast<int>(bytes.size())},
                 {"max_packet_bytes", config.max_packet_bytes}});
            return;
        }

        realnet::RelayPacket packet;
        std::string err;
        if (!realnet::decode_relay_packet(bytes, packet, err)) {
            count_drop();
            log("relay_packet_reject", {{"source", source}, {"reason", err}});
            return;
        }
        if (!room_limiter.allow(packet.room_code, now)) {
            count_drop(false, true);
            log("relay_rate_limit", {{"scope", "room"}, {"room_code", packet.room_code}});
            return;
        }
        if (datagram_handler) {
            datagram_handler(RelayDatagram{.packet = std::move(packet),
                                           .source = source,
                                           .from = from,
                                           .from_len = from_len,
                                           .now = now});
        }
    }
};

RelayUdpServer::RelayUdpServer()
    : impl_(new Impl()) {}

RelayUdpServer::~RelayUdpServer() {
    stop();
    delete impl_;
}

bool RelayUdpServer::start(const std::string& bind_host,
                           int port,
                           realnet::RelayServiceConfig config,
                           RelayDatagramHandler datagram_handler,
                           RelayDropHandler drop_handler,
                           RelayLogHandler log_handler,
                           std::string& err) {
#if defined(_WIN32)
    WSADATA data{};
    const int wsa_rc = WSAStartup(MAKEWORD(2, 2), &data);
    if (wsa_rc != 0) {
        err = "WSAStartup failed";
        return false;
    }
#endif
    impl_->config = config;
    impl_->ip_limiter = realnet::TokenBucketRateLimiter(
        {impl_->config.ip_packet_rate_per_sec, impl_->config.ip_packet_burst}
    );
    impl_->room_limiter = realnet::TokenBucketRateLimiter(
        {impl_->config.room_packet_rate_per_sec, impl_->config.room_packet_burst}
    );
    impl_->datagram_handler = std::move(datagram_handler);
    impl_->drop_handler = std::move(drop_handler);
    impl_->log_handler = std::move(log_handler);
    impl_->port = port;
    impl_->socket = open_bound_socket(bind_host, port, err);
    if (impl_->socket == kInvalidSocket)
        return false;
    impl_->running.store(true);
    impl_->thread = std::thread([this]() { impl_->run(); });
    return true;
}

void RelayUdpServer::stop() {
    impl_->running.store(false);
    close_socket(impl_->socket);
    impl_->socket = kInvalidSocket;
    if (impl_->thread.joinable())
        impl_->thread.join();
#if defined(_WIN32)
    WSACleanup();
#endif
}

int RelayUdpServer::port() const {
    return impl_->port;
}

const realnet::RelayServiceConfig& RelayUdpServer::config() const {
    return impl_->config;
}

void RelayUdpServer::send_packet(const sockaddr_storage& to,
                                 socklen_t to_len,
                                 realnet::RelayPacket packet,
                                 const std::string& key) {
    if (impl_->socket == kInvalidSocket)
        return;
    if (packet.ts_ms == 0)
        packet.ts_ms = realnet::unix_time_ms();
    realnet::sign_relay_packet(packet, key);
    const std::string bytes = realnet::encode_relay_packet(packet);
    if (bytes.empty())
        return;
    sendto(impl_->socket,
           bytes.data(),
           static_cast<int>(bytes.size()),
           0,
           reinterpret_cast<const sockaddr*>(&to),
           to_len);
}

} // namespace room_server
