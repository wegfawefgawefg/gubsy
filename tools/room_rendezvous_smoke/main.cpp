#include "../room_server/realnet_rendezvous.hpp"

#include <chrono>
#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << "room_rendezvous_smoke: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    realnet::Packet packet;
    packet.kind = realnet::PacketKind::JoinerHello;
    packet.room_code = "ABC123";
    packet.join_attempt_id = "JOIN123";
    packet.role = "joiner";
    packet.nonce = "nonce";
    packet.seq = 7;
    packet.ts_ms = 42;
    packet.endpoint = realnet::Endpoint{"127.0.0.1", 35355};
    realnet::sign_packet(packet, "secret");

    const std::string encoded = realnet::encode_packet(packet);
    realnet::Packet decoded;
    std::string err;
    if (!realnet::decode_packet(encoded, decoded, err))
        return fail(err.c_str());
    if (!realnet::verify_packet(decoded, "secret"))
        return fail("signed packet did not verify");
    decoded.room_code = "BAD123";
    if (realnet::verify_packet(decoded, "secret"))
        return fail("tampered packet verified");

    realnet::TokenBucketRateLimiter limiter({1.0, 2.0});
    const auto start = std::chrono::steady_clock::now();
    if (!limiter.allow("ip", start))
        return fail("first token rejected");
    if (!limiter.allow("ip", start))
        return fail("burst token rejected");
    if (limiter.allow("ip", start))
        return fail("rate limiter allowed past burst");
    if (!limiter.allow("ip", start + std::chrono::seconds(1)))
        return fail("rate limiter did not refill");

    std::cout << "room_rendezvous_smoke: ok\n";
    return 0;
}
