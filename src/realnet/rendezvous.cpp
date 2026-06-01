#include "gubsy/realnet/rendezvous.hpp"

#include "gubsy/realnet/crypto.hpp"

#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

namespace realnet {
namespace {

std::string json_string(const nlohmann::json& body, const char* key) {
    auto it = body.find(key);
    if (it != body.end() && it->is_string())
        return it->get<std::string>();
    return {};
}

std::uint64_t json_u64(const nlohmann::json& body, const char* key) {
    auto it = body.find(key);
    if (it != body.end() && it->is_number_unsigned())
        return it->get<std::uint64_t>();
    if (it != body.end() && it->is_number_integer()) {
        const auto value = it->get<std::int64_t>();
        return value < 0 ? 0U : static_cast<std::uint64_t>(value);
    }
    return 0;
}

nlohmann::json endpoint_json(const std::optional<Endpoint>& endpoint) {
    if (!endpoint)
        return nullptr;
    return nlohmann::json{{"host", endpoint->host}, {"port", endpoint->port}};
}

std::optional<Endpoint> endpoint_from_json(const nlohmann::json& body, const char* key) {
    auto it = body.find(key);
    if (it == body.end() || !it->is_object())
        return std::nullopt;
    Endpoint endpoint;
    endpoint.host = json_string(*it, "host");
    const auto port = json_u64(*it, "port");
    if (endpoint.host.empty() || port == 0 || port > 65535)
        return std::nullopt;
    endpoint.port = static_cast<std::uint16_t>(port);
    return endpoint;
}

} // namespace

std::string packet_kind_name(PacketKind kind) {
    switch (kind) {
    case PacketKind::HostHello:
        return "host_hello";
    case PacketKind::JoinerHello:
        return "joiner_hello";
    case PacketKind::EndpointHint:
        return "endpoint_hint";
    case PacketKind::PunchProbe:
        return "punch_probe";
    case PacketKind::PunchAck:
        return "punch_ack";
    case PacketKind::PunchResult:
        return "punch_result";
    case PacketKind::Unknown:
    default:
        return "unknown";
    }
}

PacketKind packet_kind_from_name(const std::string& name) {
    if (name == "host_hello")
        return PacketKind::HostHello;
    if (name == "joiner_hello")
        return PacketKind::JoinerHello;
    if (name == "endpoint_hint")
        return PacketKind::EndpointHint;
    if (name == "punch_probe")
        return PacketKind::PunchProbe;
    if (name == "punch_ack")
        return PacketKind::PunchAck;
    if (name == "punch_result")
        return PacketKind::PunchResult;
    return PacketKind::Unknown;
}

std::string endpoint_to_string(const Endpoint& endpoint) {
    return endpoint.host + ":" + std::to_string(endpoint.port);
}

std::optional<Endpoint> parse_endpoint(const std::string& text) {
    const auto colon = text.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= text.size())
        return std::nullopt;
    try {
        const int port = std::stoi(text.substr(colon + 1));
        if (port <= 0 || port > 65535)
            return std::nullopt;
        return Endpoint{text.substr(0, colon), static_cast<std::uint16_t>(port)};
    } catch (...) {
        return std::nullopt;
    }
}

std::uint64_t unix_time_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count());
}

std::string canonical_payload(const Packet& packet) {
    nlohmann::ordered_json body;
    body["endpoint"] = endpoint_json(packet.endpoint);
    body["join_attempt_id"] = packet.join_attempt_id;
    body["kind"] = packet_kind_name(packet.kind);
    body["nonce"] = packet.nonce;
    body["peer_endpoint"] = endpoint_json(packet.peer_endpoint);
    body["punch_secret"] = packet.punch_secret;
    body["result"] = packet.result;
    body["role"] = packet.role;
    body["room_code"] = packet.room_code;
    body["seq"] = packet.seq;
    body["ts_ms"] = packet.ts_ms;
    body["version"] = packet.version;
    return body.dump();
}

void sign_packet(Packet& packet, const std::string& key) {
    packet.mac = hmac_sha256_hex(key, canonical_payload(packet), kRendezvousMacBytes);
}

bool verify_packet(const Packet& packet, const std::string& key) {
    if (packet.mac.empty())
        return false;
    const std::string expected =
        hmac_sha256_hex(key, canonical_payload(packet), kRendezvousMacBytes);
    return constant_time_equal(packet.mac, expected);
}

std::string encode_packet(const Packet& packet) {
    nlohmann::ordered_json body = nlohmann::ordered_json::parse(canonical_payload(packet));
    body["mac"] = packet.mac;
    return body.dump();
}

bool decode_packet(const std::string& bytes, Packet& out, std::string& err) {
    if (bytes.empty() || bytes.size() > kMaxRendezvousPacketBytes) {
        err = "packet size rejected";
        return false;
    }
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(bytes);
    } catch (const std::exception& e) {
        err = std::string("invalid json: ") + e.what();
        return false;
    }
    if (!body.is_object()) {
        err = "packet must be an object";
        return false;
    }
    Packet packet;
    packet.version = static_cast<int>(json_u64(body, "version"));
    packet.kind = packet_kind_from_name(json_string(body, "kind"));
    packet.room_code = json_string(body, "room_code");
    packet.join_attempt_id = json_string(body, "join_attempt_id");
    packet.role = json_string(body, "role");
    packet.nonce = json_string(body, "nonce");
    packet.seq = json_u64(body, "seq");
    packet.ts_ms = json_u64(body, "ts_ms");
    packet.endpoint = endpoint_from_json(body, "endpoint");
    packet.peer_endpoint = endpoint_from_json(body, "peer_endpoint");
    packet.punch_secret = json_string(body, "punch_secret");
    packet.result = json_string(body, "result");
    packet.mac = json_string(body, "mac");
    if (packet.version != 1) {
        err = "unsupported packet version";
        return false;
    }
    if (packet.kind == PacketKind::Unknown) {
        err = "unknown packet kind";
        return false;
    }
    if (packet.room_code.empty()) {
        err = "missing room_code";
        return false;
    }
    out = std::move(packet);
    return true;
}

TokenBucketRateLimiter::TokenBucketRateLimiter(TokenBucketConfig config) : config_(config) {}

bool TokenBucketRateLimiter::allow(const std::string& key, std::chrono::steady_clock::time_point now) {
    auto& bucket = buckets_[key];
    if (bucket.updated.time_since_epoch().count() == 0) {
        bucket.tokens = config_.burst;
        bucket.updated = now;
    } else {
        const double elapsed =
            std::chrono::duration<double>(now - bucket.updated).count();
        bucket.tokens = std::min(config_.burst, bucket.tokens + elapsed * config_.rate_per_sec);
        bucket.updated = now;
    }
    if (bucket.tokens < 1.0)
        return false;
    bucket.tokens -= 1.0;
    return true;
}

} // namespace realnet
