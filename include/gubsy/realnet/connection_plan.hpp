#pragma once

#include "gubsy/lobby/matchmaking.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace realnet {

enum class AddressFamily {
    Unknown,
    Ipv4,
    Ipv6,
};

enum class AddressScope {
    Unknown,
    Loopback,
    Private,
    LinkLocal,
    Public,
};

struct LocalInterfaceAddress {
    std::string address;
    AddressFamily family{AddressFamily::Unknown};
    AddressScope scope{AddressScope::Unknown};
    int prefix_length{-1};
};

struct LocalNetworkInfo {
    std::vector<LocalInterfaceAddress> interfaces;
};

enum class CandidateDecision {
    Try,
    SkipInvalidEndpoint,
    SkipUnreachablePrivate,
    SkipUnsupported,
    SkipDisabled,
};

struct PlannedConnectionCandidate {
    ConnectionCandidate candidate;
    CandidateDecision decision{CandidateDecision::Try};
    ConnectPhase phase{ConnectPhase::Idle};
    std::string host;
    std::uint16_t port{0};
    std::string reason;
};

struct ConnectionPlanInput {
    MatchmakingRoom room;
    std::string join_attempt_id;
    std::string punch_secret;
    bool nat_punch_supported{false};
    bool relay_supported{false};
    bool steam_supported{false};
    bool force_nat_punch{false};
    LocalNetworkInfo local_network;
};

struct ConnectionPlan {
    std::string room_code;
    std::string join_attempt_id;
    std::vector<PlannedConnectionCandidate> candidates;
};

const char* address_family_id(AddressFamily family);
const char* address_scope_id(AddressScope scope);
const char* candidate_decision_id(CandidateDecision decision);

bool parse_endpoint(const std::string& endpoint, std::string& host, std::uint16_t& port);
AddressFamily classify_address_family(const std::string& host);
AddressScope classify_address_scope(const std::string& host);
LocalNetworkInfo detect_local_network_info();
ConnectionPlan build_connection_plan(const ConnectionPlanInput& input);

} // namespace realnet
