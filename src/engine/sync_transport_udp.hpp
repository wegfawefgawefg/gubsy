#pragma once

#include <string>
#include <utility>
#include <vector>

#include "engine/net_transport.hpp"

class UdpJsonNetTransport final : public INetTransport {
public:
    ~UdpJsonNetTransport() override;

    void reset() override;
    bool ensure_host(const std::string& room_code, std::string& err) override;
    bool ensure_client(const std::string& room_code,
                       const std::string& remote_endpoint,
                       std::string& err) override;
    bool send(const NetTransportPacket& packet, std::string& err) override;
    bool poll(std::vector<NetTransportPacket>& out, std::string& err) override;
    const std::string& public_endpoint() const override;

private:
    bool open_socket(bool is_host,
                     const std::string& room_code,
                     const std::string& remote_endpoint,
                     std::string& err);

    bool open_{false};
    bool is_host_{false};
    int socket_fd_{-1};
    std::string room_code_;
    std::string remote_endpoint_;
    std::string public_endpoint_;
    std::vector<std::pair<std::string, std::string>> member_endpoints_;
};
