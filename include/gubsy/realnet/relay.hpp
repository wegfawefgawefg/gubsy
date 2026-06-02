#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace realnet {

constexpr std::size_t kMaxRelayPacketBytes = 1400;
constexpr std::size_t kRelayMacBytes = 16;

enum class RelayPacketKind {
    Unknown,
    Hello,
    Ready,
    Keepalive,
    Close,
    Error,
    Data,
};

enum class RelayRole {
    Unknown,
    Host,
    Joiner,
};

struct RelayPacket {
    int version{1};
    RelayPacketKind kind{RelayPacketKind::Unknown};
    RelayRole role{RelayRole::Unknown};
    std::uint64_t seq{0};
    std::uint64_t ts_ms{0};
    std::string room_code;
    std::string allocation_id;
    std::string join_attempt_id;
    std::string member_id;
    std::string error;
    std::vector<std::uint8_t> payload;
    std::string mac;
};

std::string relay_packet_kind_name(RelayPacketKind kind);
RelayPacketKind relay_packet_kind_from_name(const std::string& name);
std::string relay_role_name(RelayRole role);
RelayRole relay_role_from_name(const std::string& name);
std::uint8_t relay_packet_kind_id(RelayPacketKind kind);
RelayPacketKind relay_packet_kind_from_id(std::uint8_t id);
std::uint8_t relay_role_id(RelayRole role);
RelayRole relay_role_from_id(std::uint8_t id);

std::string relay_auth_payload(const RelayPacket& packet);
void sign_relay_packet(RelayPacket& packet, const std::string& key);
bool verify_relay_packet(const RelayPacket& packet, const std::string& key);
std::string encode_relay_packet(const RelayPacket& packet);
bool decode_relay_packet(const std::string& bytes, RelayPacket& out, std::string& err);

} // namespace realnet
