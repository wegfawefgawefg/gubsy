#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

enum class NetPacketKind {
    Input,
    Snapshot,
};

struct NetTransportPacket {
    NetPacketKind kind{NetPacketKind::Input};
    std::string room_code;
    std::string member_id;
    std::uint64_t seq{0};
    nlohmann::json payload = nlohmann::json::object();
};

struct INetTransport {
    virtual ~INetTransport() = default;
    virtual void reset() = 0;
    virtual bool ensure_host(const std::string& room_code, std::string& err) = 0;
    virtual bool ensure_client(const std::string& room_code,
                               const std::string& remote_endpoint,
                               std::string& err) = 0;
    virtual bool send(const NetTransportPacket& packet, std::string& err) = 0;
    virtual bool poll(std::vector<NetTransportPacket>& out, std::string& err) = 0;
    virtual const std::string& public_endpoint() const = 0;
};
