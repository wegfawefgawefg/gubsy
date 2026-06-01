#pragma once

#include "gubsy/lobby/matchmaking.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct DirectConnectionCandidate {
    ConnectionCandidate candidate;
    std::string host;
    std::uint16_t port{0};
};

bool gubsy_parse_endpoint(const std::string& endpoint, std::string& host, std::uint16_t& port);
ConnectPhase gubsy_connect_phase_for_candidate(ConnectionCandidateKind kind);
std::vector<ConnectionCandidate> gubsy_sorted_connection_candidates(const MatchmakingRoom& room);
std::optional<DirectConnectionCandidate> gubsy_first_direct_connection_candidate(
    const MatchmakingRoom& room);
