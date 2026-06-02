#include "gubsy/realnet/connection_plan.hpp"

#include <iostream>

namespace {

int fail(const char* message) {
    std::cerr << "realnet_connection_plan_smoke: " << message << '\n';
    return 1;
}

ConnectionCandidate candidate(ConnectionCandidateKind kind, int priority,
                              const char* endpoint, const char* label) {
    ConnectionCandidate out;
    out.kind = kind;
    out.priority = priority;
    out.endpoint = endpoint;
    out.label = label;
    return out;
}

MatchmakingRoom room_with_candidates() {
    MatchmakingRoom room;
    room.room_code = "ROOM42";
    room.contract.connection_candidates.push_back(
        candidate(ConnectionCandidateKind::LanDirect, 100, "192.168.11.7:35355", "LAN"));
    room.contract.connection_candidates.push_back(
        candidate(ConnectionCandidateKind::NatPunch, 200, "", "NAT"));
    room.contract.connection_candidates.push_back(
        candidate(ConnectionCandidateKind::Relay, 300, "", "Relay"));
    return room;
}

} // namespace

int main() {
    if (realnet::classify_address_family("192.168.11.7") != realnet::AddressFamily::Ipv4)
        return fail("IPv4 address family not detected");
    if (realnet::classify_address_scope("192.168.11.7") != realnet::AddressScope::Private)
        return fail("private IPv4 scope not detected");
    if (realnet::classify_address_scope("8.8.8.8") != realnet::AddressScope::Public)
        return fail("public IPv4 scope not detected");
    if (realnet::classify_address_scope("::1") != realnet::AddressScope::Loopback)
        return fail("IPv6 loopback scope not detected");
    if (realnet::classify_address_scope("fe80::1") != realnet::AddressScope::LinkLocal)
        return fail("IPv6 link-local scope not detected");

    realnet::ConnectionPlanInput input;
    input.room = room_with_candidates();
    input.join_attempt_id = "JOIN42";
    input.punch_secret = "secret";
    input.nat_punch_supported = true;
    realnet::LocalInterfaceAddress local;
    local.address = "10.0.0.5";
    local.family = realnet::AddressFamily::Ipv4;
    local.scope = realnet::AddressScope::Private;
    local.prefix_length = 24;
    input.local_network.interfaces.push_back(local);

    const realnet::ConnectionPlan plan = realnet::build_connection_plan(input);
    if (plan.candidates.size() != 3)
        return fail("expected three candidates");
    if (plan.candidates[0].decision != realnet::CandidateDecision::SkipUnreachablePrivate)
        return fail("unreachable private direct candidate was not skipped");
    if (plan.candidates[1].decision != realnet::CandidateDecision::Try ||
        plan.candidates[1].candidate.kind != ConnectionCandidateKind::NatPunch) {
        return fail("NAT punch candidate was not available");
    }
    if (plan.candidates[2].decision != realnet::CandidateDecision::SkipDisabled)
        return fail("relay candidate should be disabled");

    input.local_network.interfaces.front().address = "192.168.11.23";
    const realnet::ConnectionPlan local_plan = realnet::build_connection_plan(input);
    if (local_plan.candidates[0].decision != realnet::CandidateDecision::Try)
        return fail("local private direct candidate was not eligible");

    input.force_nat_punch = true;
    const realnet::ConnectionPlan forced_plan = realnet::build_connection_plan(input);
    if (forced_plan.candidates.empty() ||
        forced_plan.candidates[0].candidate.kind != ConnectionCandidateKind::NatPunch) {
        return fail("forced NAT punch was not ordered first");
    }

    std::cout << "realnet_connection_plan_smoke: ok\n";
    return 0;
}
