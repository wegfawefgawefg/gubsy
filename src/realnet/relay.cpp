#include "gubsy/realnet/relay.hpp"

#include "gubsy/realnet/crypto.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace realnet {
namespace {

constexpr std::array<char, 4> kRelayMagic = {'G', 'R', 'L', 'Y'};
constexpr std::size_t kFixedHeaderBytes = 4 + 4 + 8 + 8 + 6 * 2;

void append_u16(std::string& out, std::uint16_t value) {
    out.push_back(static_cast<char>((value >> 8) & 0xffU));
    out.push_back(static_cast<char>(value & 0xffU));
}

void append_u64(std::string& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<char>((value >> shift) & 0xffU));
}

std::uint16_t read_u16(const std::string& bytes, std::size_t& offset) {
    const auto hi = static_cast<std::uint8_t>(bytes[offset++]);
    const auto lo = static_cast<std::uint8_t>(bytes[offset++]);
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(hi) << 8) | lo);
}

std::uint64_t read_u64(const std::string& bytes, std::size_t& offset) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value = (value << 8) | static_cast<std::uint8_t>(bytes[offset++]);
    return value;
}

bool append_sized_string(std::string& out, const std::string& text) {
    if (text.size() > 65535U)
        return false;
    append_u16(out, static_cast<std::uint16_t>(text.size()));
    out.append(text);
    return true;
}

bool append_sized_payload(std::string& out, const std::vector<std::uint8_t>& payload) {
    if (payload.size() > 65535U)
        return false;
    append_u16(out, static_cast<std::uint16_t>(payload.size()));
    if (!payload.empty())
        out.append(reinterpret_cast<const char*>(payload.data()), payload.size());
    return true;
}

bool read_sized_string(const std::string& bytes, std::size_t& offset, std::string& out) {
    if (offset + 2U > bytes.size())
        return false;
    const std::uint16_t size = read_u16(bytes, offset);
    if (offset + size > bytes.size())
        return false;
    out.assign(bytes.data() + offset, size);
    offset += size;
    return true;
}

bool read_sized_payload(const std::string& bytes,
                        std::size_t& offset,
                        std::vector<std::uint8_t>& out) {
    if (offset + 2U > bytes.size())
        return false;
    const std::uint16_t size = read_u16(bytes, offset);
    if (offset + size > bytes.size())
        return false;
    out.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
               bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += size;
    return true;
}

std::string encode_without_mac(const RelayPacket& packet) {
    std::string out;
    out.reserve(kFixedHeaderBytes + packet.room_code.size() + packet.allocation_id.size() +
                packet.join_attempt_id.size() + packet.member_id.size() + packet.error.size() +
                packet.payload.size());
    out.append(kRelayMagic.data(), kRelayMagic.size());
    out.push_back(static_cast<char>(packet.version));
    out.push_back(static_cast<char>(relay_packet_kind_id(packet.kind)));
    out.push_back(static_cast<char>(relay_role_id(packet.role)));
    out.push_back(0);
    append_u64(out, packet.seq);
    append_u64(out, packet.ts_ms);
    if (!append_sized_string(out, packet.room_code) ||
        !append_sized_string(out, packet.allocation_id) ||
        !append_sized_string(out, packet.join_attempt_id) ||
        !append_sized_string(out, packet.member_id) ||
        !append_sized_string(out, packet.error) ||
        !append_sized_payload(out, packet.payload)) {
        return {};
    }
    return out;
}

} // namespace

std::string relay_packet_kind_name(RelayPacketKind kind) {
    switch (kind) {
    case RelayPacketKind::Hello:
        return "relay_hello";
    case RelayPacketKind::Ready:
        return "relay_ready";
    case RelayPacketKind::Keepalive:
        return "relay_keepalive";
    case RelayPacketKind::Close:
        return "relay_close";
    case RelayPacketKind::Error:
        return "relay_error";
    case RelayPacketKind::Data:
        return "relay_data";
    case RelayPacketKind::Unknown:
    default:
        return "unknown";
    }
}

RelayPacketKind relay_packet_kind_from_name(const std::string& name) {
    if (name == "relay_hello")
        return RelayPacketKind::Hello;
    if (name == "relay_ready")
        return RelayPacketKind::Ready;
    if (name == "relay_keepalive")
        return RelayPacketKind::Keepalive;
    if (name == "relay_close")
        return RelayPacketKind::Close;
    if (name == "relay_error")
        return RelayPacketKind::Error;
    if (name == "relay_data")
        return RelayPacketKind::Data;
    return RelayPacketKind::Unknown;
}

std::string relay_role_name(RelayRole role) {
    switch (role) {
    case RelayRole::Host:
        return "host";
    case RelayRole::Joiner:
        return "joiner";
    case RelayRole::Unknown:
    default:
        return "unknown";
    }
}

RelayRole relay_role_from_name(const std::string& name) {
    if (name == "host")
        return RelayRole::Host;
    if (name == "joiner")
        return RelayRole::Joiner;
    return RelayRole::Unknown;
}

std::uint8_t relay_packet_kind_id(RelayPacketKind kind) {
    switch (kind) {
    case RelayPacketKind::Hello:
        return 1;
    case RelayPacketKind::Ready:
        return 2;
    case RelayPacketKind::Keepalive:
        return 3;
    case RelayPacketKind::Close:
        return 4;
    case RelayPacketKind::Error:
        return 5;
    case RelayPacketKind::Data:
        return 6;
    case RelayPacketKind::Unknown:
    default:
        return 0;
    }
}

RelayPacketKind relay_packet_kind_from_id(std::uint8_t id) {
    switch (id) {
    case 1:
        return RelayPacketKind::Hello;
    case 2:
        return RelayPacketKind::Ready;
    case 3:
        return RelayPacketKind::Keepalive;
    case 4:
        return RelayPacketKind::Close;
    case 5:
        return RelayPacketKind::Error;
    case 6:
        return RelayPacketKind::Data;
    default:
        return RelayPacketKind::Unknown;
    }
}

std::uint8_t relay_role_id(RelayRole role) {
    switch (role) {
    case RelayRole::Host:
        return 1;
    case RelayRole::Joiner:
        return 2;
    case RelayRole::Unknown:
    default:
        return 0;
    }
}

RelayRole relay_role_from_id(std::uint8_t id) {
    switch (id) {
    case 1:
        return RelayRole::Host;
    case 2:
        return RelayRole::Joiner;
    default:
        return RelayRole::Unknown;
    }
}

std::string relay_auth_payload(const RelayPacket& packet) {
    return encode_without_mac(packet);
}

void sign_relay_packet(RelayPacket& packet, const std::string& key) {
    const std::string payload = relay_auth_payload(packet);
    const std::vector<std::uint8_t> digest = hmac_sha256(key, payload);
    packet.mac.assign(reinterpret_cast<const char*>(digest.data()),
                      std::min(kRelayMacBytes, digest.size()));
}

bool verify_relay_packet(const RelayPacket& packet, const std::string& key) {
    if (packet.mac.size() != kRelayMacBytes)
        return false;
    RelayPacket signed_packet = packet;
    sign_relay_packet(signed_packet, key);
    return constant_time_equal(packet.mac, signed_packet.mac);
}

std::string encode_relay_packet(const RelayPacket& packet) {
    std::string out = encode_without_mac(packet);
    if (out.empty() || packet.mac.size() != kRelayMacBytes)
        return {};
    out.append(packet.mac);
    return out;
}

bool decode_relay_packet(const std::string& bytes, RelayPacket& out, std::string& err) {
    if (bytes.size() < kFixedHeaderBytes + kRelayMacBytes ||
        bytes.size() > kMaxRelayPacketBytes) {
        err = "packet size rejected";
        return false;
    }
    if (!std::equal(kRelayMagic.begin(), kRelayMagic.end(), bytes.begin())) {
        err = "invalid relay magic";
        return false;
    }
    const std::size_t body_end = bytes.size() - kRelayMacBytes;
    std::size_t offset = kRelayMagic.size();
    RelayPacket packet;
    packet.version = static_cast<std::uint8_t>(bytes[offset++]);
    packet.kind = relay_packet_kind_from_id(static_cast<std::uint8_t>(bytes[offset++]));
    packet.role = relay_role_from_id(static_cast<std::uint8_t>(bytes[offset++]));
    offset += 1;
    packet.seq = read_u64(bytes, offset);
    packet.ts_ms = read_u64(bytes, offset);
    if (!read_sized_string(bytes, offset, packet.room_code) ||
        !read_sized_string(bytes, offset, packet.allocation_id) ||
        !read_sized_string(bytes, offset, packet.join_attempt_id) ||
        !read_sized_string(bytes, offset, packet.member_id) ||
        !read_sized_string(bytes, offset, packet.error) ||
        !read_sized_payload(bytes, offset, packet.payload)) {
        err = "malformed relay packet fields";
        return false;
    }
    if (offset != body_end) {
        err = "relay packet trailing bytes";
        return false;
    }
    packet.mac.assign(bytes.data() + body_end, kRelayMacBytes);
    if (packet.version != 1) {
        err = "unsupported relay packet version";
        return false;
    }
    if (packet.kind == RelayPacketKind::Unknown) {
        err = "unknown relay packet kind";
        return false;
    }
    if (packet.role == RelayRole::Unknown) {
        err = "unknown relay packet role";
        return false;
    }
    if (packet.room_code.empty()) {
        err = "missing room_code";
        return false;
    }
    out = std::move(packet);
    return true;
}

} // namespace realnet
