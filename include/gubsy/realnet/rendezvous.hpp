#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace realnet {

constexpr std::size_t kMaxRendezvousPacketBytes = 1400;
constexpr std::size_t kRendezvousMacBytes = 16;

enum class PacketKind {
    Unknown,
    HostHello,
    JoinerHello,
    EndpointHint,
    PunchProbe,
    PunchAck,
    PunchResult,
};

struct Endpoint {
    std::string host;
    std::uint16_t port{0};
};

struct Packet {
    int version{1};
    PacketKind kind{PacketKind::Unknown};
    std::string room_code;
    std::string join_attempt_id;
    std::string role;
    std::string nonce;
    std::uint64_t seq{0};
    std::uint64_t ts_ms{0};
    std::optional<Endpoint> endpoint;
    std::optional<Endpoint> peer_endpoint;
    std::string punch_secret;
    std::string result;
    std::string mac;
};

std::string packet_kind_name(PacketKind kind);
PacketKind packet_kind_from_name(const std::string& name);
std::string endpoint_to_string(const Endpoint& endpoint);
std::optional<Endpoint> parse_endpoint(const std::string& text);
std::uint64_t unix_time_ms();

std::string canonical_payload(const Packet& packet);
void sign_packet(Packet& packet, const std::string& key);
bool verify_packet(const Packet& packet, const std::string& key);
std::string encode_packet(const Packet& packet);
bool decode_packet(const std::string& bytes, Packet& out, std::string& err);

struct TokenBucketConfig {
    double rate_per_sec{100.0};
    double burst{200.0};
};

class TokenBucketRateLimiter {
public:
    explicit TokenBucketRateLimiter(TokenBucketConfig config);
    bool allow(const std::string& key, std::chrono::steady_clock::time_point now);

private:
    struct Bucket {
        double tokens{0.0};
        std::chrono::steady_clock::time_point updated{};
    };
    TokenBucketConfig config_;
    std::unordered_map<std::string, Bucket> buckets_;
};

} // namespace realnet
