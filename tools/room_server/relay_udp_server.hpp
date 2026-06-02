#pragma once

#include "gubsy/realnet/config.hpp"
#include "gubsy/realnet/relay.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

namespace room_server {

using RelayClock = std::chrono::steady_clock;

struct RelayDatagram {
    realnet::RelayPacket packet;
    std::string source;
    sockaddr_storage from{};
    socklen_t from_len{0};
    RelayClock::time_point now{};
};

using RelayDatagramHandler = std::function<void(const RelayDatagram&)>;
using RelayDropHandler = std::function<void(bool auth_failure, bool rate_limited)>;
using RelayLogHandler = std::function<void(const char* event, const nlohmann::json& fields)>;

class RelayUdpServer {
public:
    RelayUdpServer();
    ~RelayUdpServer();

    RelayUdpServer(const RelayUdpServer&) = delete;
    RelayUdpServer& operator=(const RelayUdpServer&) = delete;

    bool start(const std::string& bind_host,
               int port,
               realnet::RelayServiceConfig config,
               RelayDatagramHandler datagram_handler,
               RelayDropHandler drop_handler,
               RelayLogHandler log_handler,
               std::string& err);
    void stop();

    int port() const;
    const realnet::RelayServiceConfig& config() const;

    void send_packet(const sockaddr_storage& to,
                     socklen_t to_len,
                     realnet::RelayPacket packet,
                     const std::string& key);

private:
    struct Impl;
    Impl* impl_{nullptr};
};

} // namespace room_server
