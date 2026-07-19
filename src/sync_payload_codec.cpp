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
        // Older nlohmann/json releases instantiate std::char_traits<uint8_t>
        // for byte-vector input. libc++ correctly leaves that specialization
        // undefined, so present the identical CBOR bytes as ordinary chars.
        const std::vector<char> bytes(payload.begin(), payload.end());
        out = nlohmann::json::from_cbor(bytes);
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        out = nlohmann::json::object();
        return false;
    }
}
