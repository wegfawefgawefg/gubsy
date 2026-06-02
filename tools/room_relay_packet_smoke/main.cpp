#include "gubsy/realnet/relay.hpp"

#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << "room_relay_packet_smoke: " << message << '\n';
    return 1;
}

} // namespace

int main() {
    realnet::RelayPacket packet;
    packet.kind = realnet::RelayPacketKind::Data;
    packet.role = realnet::RelayRole::Joiner;
    packet.room_code = "ABC123";
    packet.allocation_id = "ALLOC123";
    packet.join_attempt_id = "JOIN123";
    packet.member_id = "MEMBER123";
    packet.seq = 7;
    packet.ts_ms = 42;
    packet.payload = {0x00, 0xff, 0x7f, 0x20, 0x01};
    realnet::sign_relay_packet(packet, "relay-secret");

    const std::string encoded = realnet::encode_relay_packet(packet);
    if (encoded.empty())
        return fail("encoded relay packet was empty");

    realnet::RelayPacket decoded;
    std::string err;
    if (!realnet::decode_relay_packet(encoded, decoded, err))
        return fail(err.c_str());
    if (!realnet::verify_relay_packet(decoded, "relay-secret"))
        return fail("signed relay packet did not verify");
    if (decoded.kind != realnet::RelayPacketKind::Data ||
        decoded.role != realnet::RelayRole::Joiner ||
        decoded.payload != packet.payload) {
        return fail("relay packet did not round trip");
    }

    realnet::RelayPacket tampered = decoded;
    tampered.payload[1] = 0x02;
    if (realnet::verify_relay_packet(tampered, "relay-secret"))
        return fail("tampered relay payload verified");

    std::string corrupted = encoded;
    corrupted[4] = 99;
    if (realnet::decode_relay_packet(corrupted, decoded, err))
        return fail("unsupported relay packet version decoded");

    realnet::RelayPacket ready;
    ready.kind = realnet::RelayPacketKind::Ready;
    ready.role = realnet::RelayRole::Host;
    ready.room_code = "ABC123";
    ready.allocation_id = "ALLOC123";
    ready.seq = 8;
    ready.ts_ms = 43;
    realnet::sign_relay_packet(ready, "host-secret");
    if (!realnet::decode_relay_packet(realnet::encode_relay_packet(ready), decoded, err))
        return fail(err.c_str());
    if (realnet::relay_packet_kind_name(decoded.kind) != "relay_ready" ||
        realnet::relay_role_name(decoded.role) != "host") {
        return fail("relay ready metadata did not decode");
    }

    std::cout << "room_relay_packet_smoke: ok\n";
    return 0;
}
