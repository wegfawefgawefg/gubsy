#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct CoopSequencedInput {
    std::uint64_t seq{0};
    std::vector<std::uint8_t> payload;
};

struct CoopSnapshotEnvelope {
    std::uint64_t sim_frame{0};
    std::vector<std::pair<std::string, std::uint64_t>> acked_inputs;
    std::vector<std::uint8_t> driver_snapshot;
};

std::string coop_sync_normalized_room_code(std::string room_code);
bool coop_sync_encode_snapshot_envelope(const CoopSnapshotEnvelope& envelope,
                                        std::vector<std::uint8_t>& out,
                                        std::string& err);
bool coop_sync_decode_snapshot_envelope(const std::vector<std::uint8_t>& bytes,
                                        CoopSnapshotEnvelope& out,
                                        std::string& err);
