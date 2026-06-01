#include "src/session_contract.hpp"

#include <algorithm>

namespace {

constexpr const char* kDefaultNetProtocol = "gubsy-sync-1";

struct CandidateKindDef {
    ConnectionCandidateKind kind;
    const char* id;
};

constexpr CandidateKindDef kCandidateKinds[] = {
    {ConnectionCandidateKind::Loopback, "loopback"},
    {ConnectionCandidateKind::LanDirect, "lan_direct"},
    {ConnectionCandidateKind::PublicDirect, "public_direct"},
    {ConnectionCandidateKind::NatPunch, "nat_punch"},
    {ConnectionCandidateKind::Relay, "relay"},
    {ConnectionCandidateKind::Steam, "steam"},
};

std::vector<std::string> normalized_mod_ids(std::vector<std::string> ids) {
    ids.erase(std::remove_if(ids.begin(),
                             ids.end(),
                             [](const std::string& id) { return id.empty(); }),
              ids.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

nlohmann::json connection_candidate_to_json(const ConnectionCandidate& candidate) {
    nlohmann::json json{
        {"kind", connection_candidate_kind_id(candidate.kind)},
        {"priority", candidate.priority},
    };
    if (!candidate.endpoint.empty())
        json["endpoint"] = candidate.endpoint;
    if (!candidate.label.empty())
        json["label"] = candidate.label;
    if (!candidate.token.empty())
        json["token"] = candidate.token;
    return json;
}

bool connection_candidate_from_json(const nlohmann::json& json, ConnectionCandidate& out) {
    if (!json.is_object())
        return false;
    out = ConnectionCandidate{};
    out.kind = connection_candidate_kind_from_id(json.value("kind", "lan_direct"));
    out.priority = json.value("priority", 100);
    out.endpoint = json.value("endpoint", "");
    out.label = json.value("label", "");
    out.token = json.value("token", "");
    return true;
}

bool connection_candidates_equal(const std::vector<ConnectionCandidate>& a,
                                 const std::vector<ConnectionCandidate>& b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].kind != b[i].kind ||
            a[i].priority != b[i].priority ||
            a[i].endpoint != b[i].endpoint ||
            a[i].label != b[i].label ||
            a[i].token != b[i].token) {
            return false;
        }
    }
    return true;
}

} // namespace

const char* session_contract_default_net_protocol() {
    return kDefaultNetProtocol;
}

const char* room_authority_mode_id(RoomAuthorityMode mode) {
    switch (mode) {
        case RoomAuthorityMode::PlayerHost:
            return "player_host";
        case RoomAuthorityMode::DedicatedServer:
            return "dedicated_server";
    }
    return "player_host";
}

RoomAuthorityMode room_authority_mode_from_id(const std::string& id) {
    if (id == "dedicated_server")
        return RoomAuthorityMode::DedicatedServer;
    return RoomAuthorityMode::PlayerHost;
}

const char* connection_candidate_kind_id(ConnectionCandidateKind kind) {
    for (const CandidateKindDef& def : kCandidateKinds) {
        if (def.kind == kind)
            return def.id;
    }
    return "lan_direct";
}

ConnectionCandidateKind connection_candidate_kind_from_id(const std::string& id) {
    for (const CandidateKindDef& def : kCandidateKinds) {
        if (id == def.id)
            return def.kind;
    }
    return ConnectionCandidateKind::LanDirect;
}

const char* connect_phase_id(ConnectPhase phase) {
    switch (phase) {
        case ConnectPhase::Idle:
            return "idle";
        case ConnectPhase::ResolvingRoom:
            return "resolving_room";
        case ConnectPhase::CheckingCompatibility:
            return "checking_compatibility";
        case ConnectPhase::TryingLoopback:
            return "trying_loopback";
        case ConnectPhase::TryingLanDirect:
            return "trying_lan_direct";
        case ConnectPhase::TryingPublicDirect:
            return "trying_public_direct";
        case ConnectPhase::TryingNatPunch:
            return "trying_nat_punch";
        case ConnectPhase::TryingRelay:
            return "trying_relay";
        case ConnectPhase::TryingSteam:
            return "trying_steam";
        case ConnectPhase::Connected:
            return "connected";
        case ConnectPhase::Failed:
            return "failed";
    }
    return "idle";
}

bool session_contract_is_in_game(const SessionContract& contract) {
    return contract.session_phase == "in_game";
}

bool session_contract_equal(const SessionContract& a, const SessionContract& b) {
    return a.game_version == b.game_version &&
           a.net_protocol == b.net_protocol &&
           a.session_phase == b.session_phase &&
           a.authority_mode == b.authority_mode &&
           a.mod_hash == b.mod_hash &&
           normalized_mod_ids(a.required_mod_ids) == normalized_mod_ids(b.required_mod_ids) &&
           a.content_revision == b.content_revision &&
           a.allow_live_mod_reload == b.allow_live_mod_reload &&
           a.realtime_endpoint == b.realtime_endpoint &&
           connection_candidates_equal(a.connection_candidates, b.connection_candidates) &&
           a.game_config == b.game_config;
}

nlohmann::json session_contract_to_json(const SessionContract& contract) {
    nlohmann::json connection_candidates = nlohmann::json::array();
    for (const ConnectionCandidate& candidate : contract.connection_candidates)
        connection_candidates.push_back(connection_candidate_to_json(candidate));
    return {
        {"game_version", contract.game_version},
        {"net_protocol", contract.net_protocol.empty() ? session_contract_default_net_protocol()
                                                        : contract.net_protocol},
        {"session_phase", contract.session_phase},
        {"authority_mode", room_authority_mode_id(contract.authority_mode)},
        {"mod_hash", contract.mod_hash},
        {"required_mod_ids", normalized_mod_ids(contract.required_mod_ids)},
        {"content_revision", contract.content_revision},
        {"allow_live_mod_reload", contract.allow_live_mod_reload},
        {"realtime_endpoint", contract.realtime_endpoint},
        {"connection_candidates", std::move(connection_candidates)},
        {"game_config", contract.game_config.is_object() ? contract.game_config
                                                          : nlohmann::json::object()},
    };
}

bool session_contract_from_json(const nlohmann::json& json, SessionContract& out) {
    if (!json.is_object())
        return false;
    out = SessionContract{};
    out.game_version = json.value("game_version", "");
    out.net_protocol = json.value("net_protocol", std::string{session_contract_default_net_protocol()});
    out.session_phase = json.value("session_phase",
                                   json.value("in_game", false) ? "in_game" : "lobby");
    out.authority_mode = room_authority_mode_from_id(json.value("authority_mode", "player_host"));
    out.mod_hash = json.value("mod_hash", "");
    auto required_mods_it = json.find("required_mod_ids");
    if (required_mods_it != json.end() && required_mods_it->is_array()) {
        for (const auto& entry : *required_mods_it) {
            if (entry.is_string())
                out.required_mod_ids.push_back(entry.get<std::string>());
        }
        out.required_mod_ids = normalized_mod_ids(std::move(out.required_mod_ids));
    }
    out.content_revision = json.value("content_revision", std::uint64_t{1});
    out.allow_live_mod_reload = json.value("allow_live_mod_reload", true);
    out.realtime_endpoint = json.value("realtime_endpoint", "");
    auto candidates_it = json.find("connection_candidates");
    if (candidates_it != json.end() && candidates_it->is_array()) {
        for (const auto& candidate_json : *candidates_it) {
            ConnectionCandidate candidate;
            if (connection_candidate_from_json(candidate_json, candidate))
                out.connection_candidates.push_back(std::move(candidate));
        }
    }
    auto config_it = json.find("game_config");
    if (config_it != json.end() && config_it->is_object())
        out.game_config = *config_it;
    return true;
}

SessionCompatibility session_contract_check_compatibility(const SessionContract& remote,
                                                          const SessionContract& local) {
    if (!remote.game_version.empty() &&
        !local.game_version.empty() &&
        remote.game_version != local.game_version) {
        return SessionCompatibility::WrongGameVersion;
    }
    if (!remote.net_protocol.empty() &&
        !local.net_protocol.empty() &&
        remote.net_protocol != local.net_protocol) {
        return SessionCompatibility::WrongNetProtocol;
    }
    if (!remote.mod_hash.empty() &&
        !local.mod_hash.empty() &&
        remote.mod_hash != local.mod_hash) {
        return SessionCompatibility::NeedsContentReload;
    }
    if (!remote.required_mod_ids.empty() &&
        normalized_mod_ids(remote.required_mod_ids) != normalized_mod_ids(local.required_mod_ids)) {
        return SessionCompatibility::NeedsContentReload;
    }
    return SessionCompatibility::Compatible;
}

const char* session_contract_compatibility_text(SessionCompatibility compatibility) {
    switch (compatibility) {
        case SessionCompatibility::Compatible:
            return "Compatible";
        case SessionCompatibility::NeedsContentReload:
            return "Needs content reload";
        case SessionCompatibility::WrongGameVersion:
            return "Wrong game version";
        case SessionCompatibility::WrongNetProtocol:
            return "Wrong network protocol";
    }
    return "Unknown";
}
