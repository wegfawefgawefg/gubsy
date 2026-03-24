#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

struct SessionContract {
    std::string game_version;
    std::string net_protocol;
    std::string session_phase{"lobby"};
    std::string mod_hash;
    std::vector<std::string> required_mod_ids;
    std::uint64_t content_revision{1};
    bool allow_live_mod_reload{true};
    std::string realtime_endpoint;
    nlohmann::json game_config = nlohmann::json::object();
};

enum class SessionCompatibility {
    Compatible,
    NeedsContentReload,
    WrongGameVersion,
    WrongNetProtocol,
};

const char* session_contract_default_net_protocol();
bool session_contract_is_in_game(const SessionContract& contract);
bool session_contract_equal(const SessionContract& a, const SessionContract& b);
nlohmann::json session_contract_to_json(const SessionContract& contract);
bool session_contract_from_json(const nlohmann::json& json, SessionContract& out);
SessionCompatibility session_contract_check_compatibility(const SessionContract& remote,
                                                          const SessionContract& local);
const char* session_contract_compatibility_text(SessionCompatibility compatibility);
