#include "engine/sync_session_wire.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include <httplib/httplib.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

struct EndpointInfo {
    std::string host;
    int port{80};
};

bool parse_http_endpoint(const std::string& url, EndpointInfo& out, std::string& err) {
    std::string work = url;
    constexpr const char* prefix = "http://";
    if (work.rfind(prefix, 0) != 0) {
        err = "Only http:// room servers are supported";
        return false;
    }
    work = work.substr(7);
    auto slash = work.find('/');
    if (slash != std::string::npos)
        work = work.substr(0, slash);
    if (work.empty()) {
        err = "Missing host in room server URL";
        return false;
    }
    out.host = work;
    auto colon = work.find(':');
    if (colon != std::string::npos) {
        out.host = work.substr(0, colon);
        try {
            out.port = std::stoi(work.substr(colon + 1));
        } catch (...) {
            err = "Invalid room server port";
            return false;
        }
    }
    if (out.host.empty()) {
        err = "Invalid room server host";
        return false;
    }
    return true;
}

} // namespace

std::optional<nlohmann::json> sync_session_post_json(const std::string& server_url,
                                                     const std::string& path,
                                                     const nlohmann::json& body,
                                                     std::string& err) {
    EndpointInfo endpoint;
    if (!parse_http_endpoint(server_url, endpoint, err))
        return std::nullopt;
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);
    auto res = client.Post(path.c_str(), body.dump(), "application/json");
    if (!res) {
        err = "Failed to reach room server";
        return std::nullopt;
    }
    if (res->status < 200 || res->status >= 300) {
        err = "Room server request failed (" + std::to_string(res->status) + ")";
        if (!res->body.empty())
            err += ": " + res->body;
        return std::nullopt;
    }
    try {
        return nlohmann::json::parse(res->body);
    } catch (const std::exception& e) {
        err = e.what();
        return std::nullopt;
    }
}

std::optional<nlohmann::json> sync_session_get_json(const std::string& server_url,
                                                    const std::string& path,
                                                    std::string& err) {
    EndpointInfo endpoint;
    if (!parse_http_endpoint(server_url, endpoint, err))
        return std::nullopt;
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(3, 0);
    auto res = client.Get(path.c_str());
    if (!res) {
        err = "Failed to reach room server";
        return std::nullopt;
    }
    if (res->status < 200 || res->status >= 300) {
        err = "Room server request failed (" + std::to_string(res->status) + ")";
        if (!res->body.empty())
            err += ": " + res->body;
        return std::nullopt;
    }
    try {
        return nlohmann::json::parse(res->body);
    } catch (const std::exception& e) {
        err = e.what();
        return std::nullopt;
    }
}

std::string sync_session_normalized_room_code(std::string room_code) {
    room_code.erase(std::remove_if(room_code.begin(),
                                   room_code.end(),
                                   [](unsigned char c) { return std::isspace(c) != 0; }),
                    room_code.end());
    std::transform(room_code.begin(), room_code.end(), room_code.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return room_code;
}

SequencedInput sync_session_parse_input_envelope(const nlohmann::json& json) {
    SequencedInput input;
    if (json.is_object() && json.contains("seq") && json.contains("payload")) {
        input.seq = json.value("seq", std::uint64_t{0});
        const auto payload_it = json.find("payload");
        if (payload_it != json.end() && payload_it->is_object())
            input.payload = *payload_it;
        return input;
    }
    if (json.is_object())
        input.payload = json;
    return input;
}

nlohmann::json sync_session_make_input_envelope(const SequencedInput& input) {
    return {
        {"seq", input.seq},
        {"payload", input.payload},
    };
}
