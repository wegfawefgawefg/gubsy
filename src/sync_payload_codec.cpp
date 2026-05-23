#include "src/sync_payload_codec.hpp"

#include <exception>

bool sync_payload_encode_json(const nlohmann::json& json,
                              std::vector<std::uint8_t>& out,
                              std::string& err) {
    err.clear();
    try {
        out = nlohmann::json::to_cbor(json);
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        out.clear();
        return false;
    }
}

bool sync_payload_decode_json(const std::vector<std::uint8_t>& payload,
                              nlohmann::json& out,
                              std::string& err) {
    err.clear();
    try {
        out = nlohmann::json::from_cbor(payload);
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        out = nlohmann::json::object();
        return false;
    }
}
