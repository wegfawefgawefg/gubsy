#include "engine/session_contract.hpp"

namespace {

constexpr const char* kDefaultNetProtocol = "gubsy-sync-1";

} // namespace

const char* session_contract_default_net_protocol() {
    return kDefaultNetProtocol;
}

bool session_contract_is_in_game(const SessionContract& contract) {
    return contract.session_phase == "in_game";
}

bool session_contract_equal(const SessionContract& a, const SessionContract& b) {
    return a.game_version == b.game_version &&
           a.net_protocol == b.net_protocol &&
           a.session_phase == b.session_phase &&
           a.mod_hash == b.mod_hash &&
           a.content_revision == b.content_revision &&
           a.allow_live_mod_reload == b.allow_live_mod_reload &&
           a.realtime_endpoint == b.realtime_endpoint &&
           a.game_config == b.game_config;
}

nlohmann::json session_contract_to_json(const SessionContract& contract) {
    return {
        {"game_version", contract.game_version},
        {"net_protocol", contract.net_protocol.empty() ? session_contract_default_net_protocol()
                                                        : contract.net_protocol},
        {"session_phase", contract.session_phase},
        {"mod_hash", contract.mod_hash},
        {"content_revision", contract.content_revision},
        {"allow_live_mod_reload", contract.allow_live_mod_reload},
        {"realtime_endpoint", contract.realtime_endpoint},
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
    out.mod_hash = json.value("mod_hash", "");
    out.content_revision = json.value("content_revision", std::uint64_t{1});
    out.allow_live_mod_reload = json.value("allow_live_mod_reload", true);
    out.realtime_endpoint = json.value("realtime_endpoint", "");
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
