#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

struct SequencedInput {
    std::uint64_t seq{0};
    nlohmann::json payload = nlohmann::json::object();
};

std::optional<nlohmann::json> sync_session_post_json(const std::string& server_url,
                                                     const std::string& path,
                                                     const nlohmann::json& body,
                                                     std::string& err);
std::optional<nlohmann::json> sync_session_get_json(const std::string& server_url,
                                                    const std::string& path,
                                                    std::string& err);
std::string sync_session_normalized_room_code(std::string room_code);
SequencedInput sync_session_parse_input_envelope(const nlohmann::json& json);
nlohmann::json sync_session_make_input_envelope(const SequencedInput& input);
