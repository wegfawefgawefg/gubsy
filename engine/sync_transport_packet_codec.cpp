#include "engine/sync_transport_packet_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

constexpr std::array<std::uint8_t, 4> kMagic{{'G', 'U', 'B', 'N'}};
constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kHeaderBytes = 22;

void append_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void append_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

bool read_u16(const std::vector<std::uint8_t>& bytes, std::size_t& offset, std::uint16_t& out) {
    if (offset + 2 > bytes.size())
        return false;
    out = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) |
                                     static_cast<std::uint16_t>(bytes[offset + 1]));
    offset += 2;
    return true;
}

bool read_u32(const std::vector<std::uint8_t>& bytes, std::size_t& offset, std::uint32_t& out) {
    if (offset + 4 > bytes.size())
        return false;
    out = (static_cast<std::uint32_t>(bytes[offset]) << 24) |
          (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
          (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
          static_cast<std::uint32_t>(bytes[offset + 3]);
    offset += 4;
    return true;
}

bool read_u64(const std::vector<std::uint8_t>& bytes, std::size_t& offset, std::uint64_t& out) {
    if (offset + 8 > bytes.size())
        return false;
    out = 0;
    for (int i = 0; i < 8; ++i)
        out = (out << 8) | static_cast<std::uint64_t>(bytes[offset + static_cast<std::size_t>(i)]);
    offset += 8;
    return true;
}

bool is_valid_kind(std::uint8_t value) {
    return value <= static_cast<std::uint8_t>(NetPacketKind::Snapshot);
}

} // namespace

bool sync_transport_packet_serialize(const NetTransportPacket& packet,
                                     std::vector<std::uint8_t>& out,
                                     std::string& err) {
    err.clear();
    if (packet.room_code.size() > std::numeric_limits<std::uint16_t>::max()) {
        err = "Room code too long for realtime packet";
        return false;
    }
    if (packet.member_id.size() > std::numeric_limits<std::uint16_t>::max()) {
        err = "Member ID too long for realtime packet";
        return false;
    }
    if (packet.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        err = "Payload too large for realtime packet";
        return false;
    }

    out.clear();
    out.reserve(kHeaderBytes + packet.room_code.size() + packet.member_id.size() + packet.payload.size());
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    out.push_back(kVersion);
    out.push_back(static_cast<std::uint8_t>(packet.kind));
    append_u16(out, static_cast<std::uint16_t>(packet.room_code.size()));
    append_u16(out, static_cast<std::uint16_t>(packet.member_id.size()));
    append_u32(out, static_cast<std::uint32_t>(packet.payload.size()));
    append_u64(out, packet.seq);
    out.insert(out.end(), packet.room_code.begin(), packet.room_code.end());
    out.insert(out.end(), packet.member_id.begin(), packet.member_id.end());
    out.insert(out.end(), packet.payload.begin(), packet.payload.end());
    return true;
}

bool sync_transport_packet_deserialize(const std::vector<std::uint8_t>& bytes,
                                       NetTransportPacket& out,
                                       std::string& err) {
    err.clear();
    if (bytes.size() < kHeaderBytes) {
        err = "Realtime packet too short";
        return false;
    }
    if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
        err = "Realtime packet magic mismatch";
        return false;
    }
    if (bytes[4] != kVersion) {
        err = "Realtime packet version mismatch";
        return false;
    }
    if (!is_valid_kind(bytes[5])) {
        err = "Realtime packet kind is invalid";
        return false;
    }

    std::size_t offset = 6;
    std::uint16_t room_code_len = 0;
    std::uint16_t member_id_len = 0;
    std::uint32_t payload_len = 0;
    std::uint64_t seq = 0;
    if (!read_u16(bytes, offset, room_code_len) ||
        !read_u16(bytes, offset, member_id_len) ||
        !read_u32(bytes, offset, payload_len) ||
        !read_u64(bytes, offset, seq)) {
        err = "Realtime packet header is truncated";
        return false;
    }

    const std::size_t required =
        kHeaderBytes + static_cast<std::size_t>(room_code_len) +
        static_cast<std::size_t>(member_id_len) +
        static_cast<std::size_t>(payload_len);
    if (required != bytes.size()) {
        err = "Realtime packet size mismatch";
        return false;
    }

    out = NetTransportPacket{};
    out.kind = static_cast<NetPacketKind>(bytes[5]);
    out.seq = seq;
    out.room_code.assign(reinterpret_cast<const char*>(bytes.data() + offset),
                         static_cast<std::size_t>(room_code_len));
    offset += static_cast<std::size_t>(room_code_len);
    out.member_id.assign(reinterpret_cast<const char*>(bytes.data() + offset),
                         static_cast<std::size_t>(member_id_len));
    offset += static_cast<std::size_t>(member_id_len);
    out.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
    return true;
}
