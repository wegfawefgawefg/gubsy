#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

bool sync_payload_encode_json(const nlohmann::json& json,
                              std::vector<std::uint8_t>& out,
                              std::string& err);
bool sync_payload_decode_json(const std::vector<std::uint8_t>& payload,
                              nlohmann::json& out,
                              std::string& err);
