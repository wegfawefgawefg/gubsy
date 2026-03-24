#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/sync_session_wire.hpp"

struct SyncTransportMemberInput {
    std::string member_id;
    SequencedInput input;
};

struct SyncUdpTransport {
    bool open{false};
    bool is_host{false};
    int socket_fd{-1};
    std::string room_code;
    std::string remote_endpoint;
    std::string public_endpoint;
    std::vector<std::pair<std::string, std::string>> member_endpoints;
};

void sync_udp_transport_reset(SyncUdpTransport& transport);
bool sync_udp_transport_ensure_host(SyncUdpTransport& transport,
                                    const std::string& room_code,
                                    std::string& err);
bool sync_udp_transport_ensure_client(SyncUdpTransport& transport,
                                      const std::string& room_code,
                                      const std::string& remote_endpoint,
                                      std::string& err);
bool sync_udp_transport_send_input(SyncUdpTransport& transport,
                                   const std::string& member_id,
                                   const SequencedInput& input,
                                   std::string& err);
bool sync_udp_transport_collect_host_inputs(SyncUdpTransport& transport,
                                            std::vector<SyncTransportMemberInput>& out,
                                            std::string& err);
bool sync_udp_transport_send_snapshot(SyncUdpTransport& transport,
                                      const nlohmann::json& snapshot,
                                      std::string& err);
bool sync_udp_transport_collect_client_snapshot(SyncUdpTransport& transport,
                                                nlohmann::json& snapshot_out,
                                                bool& has_snapshot,
                                                std::string& err);
const std::string& sync_udp_transport_public_endpoint(const SyncUdpTransport& transport);
