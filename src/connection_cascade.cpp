#include "gubsy/lobby/connection_cascade.hpp"

#include <algorithm>

bool gubsy_parse_endpoint(const std::string& endpoint, std::string& host, std::uint16_t& port) {
    std::size_t colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon + 1 >= endpoint.size())
        return false;
    host = endpoint.substr(0, colon);
    if (host.empty())
        return false;
    try {
        int parsed = std::stoi(endpoint.substr(colon + 1));
        if (parsed <= 0 || parsed > 65535)
            return false;
        port = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

ConnectPhase gubsy_connect_phase_for_candidate(ConnectionCandidateKind kind) {
    switch (kind) {
        case ConnectionCandidateKind::Loopback:
            return ConnectPhase::TryingLoopback;
        case ConnectionCandidateKind::LanDirect:
            return ConnectPhase::TryingLanDirect;
        case ConnectionCandidateKind::PublicDirect:
            return ConnectPhase::TryingPublicDirect;
        case ConnectionCandidateKind::NatPunch:
            return ConnectPhase::TryingNatPunch;
        case ConnectionCandidateKind::Relay:
            return ConnectPhase::TryingRelay;
        case ConnectionCandidateKind::Steam:
            return ConnectPhase::TryingSteam;
    }
    return ConnectPhase::TryingLanDirect;
}

std::vector<ConnectionCandidate> gubsy_sorted_connection_candidates(const MatchmakingRoom& room) {
    std::vector<ConnectionCandidate> candidates = room.contract.connection_candidates;
    if (candidates.empty() && !room.contract.realtime_endpoint.empty()) {
        ConnectionCandidate candidate;
        candidate.kind = ConnectionCandidateKind::LanDirect;
        candidate.priority = 100;
        candidate.endpoint = room.contract.realtime_endpoint;
        candidate.label = "Direct UDP";
        candidates.push_back(std::move(candidate));
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const ConnectionCandidate& a, const ConnectionCandidate& b) {
                         return a.priority < b.priority;
                     });
    return candidates;
}

std::optional<DirectConnectionCandidate> gubsy_first_direct_connection_candidate(
    const MatchmakingRoom& room) {
    for (const ConnectionCandidate& candidate : gubsy_sorted_connection_candidates(room)) {
        switch (candidate.kind) {
            case ConnectionCandidateKind::Loopback:
            case ConnectionCandidateKind::LanDirect:
            case ConnectionCandidateKind::PublicDirect: {
                DirectConnectionCandidate selected;
                selected.candidate = candidate;
                if (gubsy_parse_endpoint(candidate.endpoint, selected.host, selected.port))
                    return selected;
                break;
            }
            case ConnectionCandidateKind::NatPunch:
            case ConnectionCandidateKind::Relay:
            case ConnectionCandidateKind::Steam:
                break;
        }
    }
    return std::nullopt;
}
