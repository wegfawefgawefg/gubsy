#include "engine/sync_session_wire.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include <httplib/httplib.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

struct EndpointInfo {
    std::string host;
    int port{80};
};

bool parse_http_endpoint(const std::string& url, EndpointInfo& out, std::string& err) {
    std::string work = url;
    constexpr const char* prefix = "http://";
    if (work.rfind(prefix, 0) != 0) {
        err = "Only http:// room servers are supported";
        return false;
    }
    work = work.substr(7);
    auto slash = work.find('/');
    if (slash != std::string::npos)
        work = work.substr(0, slash);
    if (work.empty()) {
        err = "Missing host in room server URL";
        return false;
    }
    out.host = work;
    auto colon = work.find(':');
    if (colon != std::string::npos) {
        out.host = work.substr(0, colon);
        try {
            out.port = std::stoi(work.substr(colon + 1));
        } catch (...) {
            err = "Invalid room server port";
            return false;
        }
    }
    if (out.host.empty()) {
        err = "Invalid room server host";
        return false;
    }
    return true;
}

} // namespace

std::optional<nlohmann::json> sync_session_post_json(const std::string& server_url,
                                                     const std::string& path,
                                                     const nlohmann::json& body,
                                                     std::string& err) {
    EndpointInfo endpoint;
    if (!parse_http_endpoint(server_url, endpoint, err))
        return std::nullopt;
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);
    auto res = client.Post(path.c_str(), body.dump(), "application/json");
    if (!res) {
        err = "Failed to reach room server";
        return std::nullopt;
    }
    if (res->status < 200 || res->status >= 300) {
        err = "Room server request failed (" + std::to_string(res->status) + ")";
        if (!res->body.empty())
            err += ": " + res->body;
        return std::nullopt;
    }
    try {
        return nlohmann::json::parse(res->body);
    } catch (const std::exception& e) {
        err = e.what();
        return std::nullopt;
    }
}

std::optional<nlohmann::json> sync_session_get_json(const std::string& server_url,
                                                    const std::string& path,
                                                    std::string& err) {
    EndpointInfo endpoint;
    if (!parse_http_endpoint(server_url, endpoint, err))
        return std::nullopt;
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);
    auto res = client.Get(path.c_str());
    if (!res) {
        err = "Failed to reach room server";
        return std::nullopt;
    }
    if (res->status < 200 || res->status >= 300) {
        err = "Room server request failed (" + std::to_string(res->status) + ")";
        if (!res->body.empty())
            err += ": " + res->body;
        return std::nullopt;
    }
    try {
        return nlohmann::json::parse(res->body);
    } catch (const std::exception& e) {
        err = e.what();
        return std::nullopt;
    }
}

std::string sync_session_normalized_room_code(std::string room_code) {
    room_code.erase(std::remove_if(room_code.begin(),
                                   room_code.end(),
                                   [](unsigned char c) { return std::isspace(c) != 0; }),
                    room_code.end());
    std::transform(room_code.begin(), room_code.end(), room_code.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return room_code;
}

namespace {

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

} // namespace

bool sync_session_encode_snapshot_envelope(const SyncSnapshotEnvelope& envelope,
                                           std::vector<std::uint8_t>& out,
                                           std::string& err) {
    err.clear();
    if (envelope.acked_inputs.size() > std::numeric_limits<std::uint16_t>::max()) {
        err = "Too many ack entries in snapshot envelope";
        return false;
    }
    if (envelope.driver_snapshot.size() > std::numeric_limits<std::uint32_t>::max()) {
        err = "Snapshot payload too large";
        return false;
    }
    out.clear();
    out.reserve(16 + envelope.driver_snapshot.size() + envelope.acked_inputs.size() * 24);
    append_u64(out, envelope.sim_frame);
    append_u16(out, static_cast<std::uint16_t>(envelope.acked_inputs.size()));
    for (const auto& ack : envelope.acked_inputs) {
        if (ack.first.size() > std::numeric_limits<std::uint16_t>::max()) {
            err = "Member ID too long in snapshot envelope";
            out.clear();
            return false;
        }
        append_u16(out, static_cast<std::uint16_t>(ack.first.size()));
        out.insert(out.end(), ack.first.begin(), ack.first.end());
        append_u64(out, ack.second);
    }
    append_u32(out, static_cast<std::uint32_t>(envelope.driver_snapshot.size()));
    out.insert(out.end(), envelope.driver_snapshot.begin(), envelope.driver_snapshot.end());
    return true;
}

bool sync_session_decode_snapshot_envelope(const std::vector<std::uint8_t>& bytes,
                                           SyncSnapshotEnvelope& out,
                                           std::string& err) {
    err.clear();
    out = SyncSnapshotEnvelope{};
    std::size_t offset = 0;
    std::uint16_t ack_count = 0;
    if (!read_u64(bytes, offset, out.sim_frame) ||
        !read_u16(bytes, offset, ack_count)) {
        err = "Snapshot envelope header is truncated";
        return false;
    }
    out.acked_inputs.reserve(ack_count);
    for (std::uint16_t i = 0; i < ack_count; ++i) {
        std::uint16_t member_len = 0;
        std::uint64_t acked_seq = 0;
        if (!read_u16(bytes, offset, member_len) ||
            offset + member_len > bytes.size()) {
            err = "Snapshot envelope member ID is truncated";
            return false;
        }
        std::string member_id(reinterpret_cast<const char*>(bytes.data() + offset),
                              static_cast<std::size_t>(member_len));
        offset += static_cast<std::size_t>(member_len);
        if (!read_u64(bytes, offset, acked_seq)) {
            err = "Snapshot envelope ack entry is truncated";
            return false;
        }
        out.acked_inputs.push_back({std::move(member_id), acked_seq});
    }

    std::uint32_t snapshot_len = 0;
    if (!read_u32(bytes, offset, snapshot_len) ||
        offset + snapshot_len != bytes.size()) {
        err = "Snapshot envelope payload is truncated";
        return false;
    }
    out.driver_snapshot.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
    return true;
}
