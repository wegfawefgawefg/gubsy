#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

struct SequencedInput {
    std::uint64_t seq{0};
    std::vector<std::uint8_t> payload;
};

struct SyncSnapshotEnvelope {
    std::uint64_t sim_frame{0};
    std::vector<std::pair<std::string, std::uint64_t>> acked_inputs;
    std::vector<std::uint8_t> driver_snapshot;
};

std::optional<nlohmann::json> sync_session_post_json(const std::string& server_url,
                                                     const std::string& path,
                                                     const nlohmann::json& body,
                                                     std::string& err);
std::optional<nlohmann::json> sync_session_get_json(const std::string& server_url,
                                                    const std::string& path,
                                                    std::string& err);
std::string sync_session_normalized_room_code(std::string room_code);
bool sync_session_encode_snapshot_envelope(const SyncSnapshotEnvelope& envelope,
                                           std::vector<std::uint8_t>& out,
                                           std::string& err);
bool sync_session_decode_snapshot_envelope(const std::vector<std::uint8_t>& bytes,
                                           SyncSnapshotEnvelope& out,
                                           std::string& err);
