#pragma once

#include "gubsy/lobby/net_transport.hpp"
#include "gubsy/lobby/session_contract.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct SessionLinkConnection {
    bool active{false};
    bool is_host{false};
    std::string server_url;
    std::string room_code;
    std::string host_secret;
    std::string local_member_id;
    SessionContract contract{};
};

struct SessionLinkHooks {
    void* ctx{nullptr};
    bool (*query_connection)(void* ctx, SessionLinkConnection& out){nullptr};
    void (*tick_presence)(void* ctx){nullptr};
    double (*query_now)(void* ctx){nullptr};
};

struct SessionLinkStats {
    bool active{false};
    bool is_host{false};
    bool transport_ready{false};
    std::uint64_t generation{0};
    double connected_for_sec{0.0};
    double packet_idle_sec{0.0};
};

void session_link_configure(const SessionLinkHooks& hooks);
void session_link_reset();
bool session_link_active();
bool session_link_update(std::string& err);
bool session_link_query_connection(SessionLinkConnection& out);
bool session_link_send(const NetTransportPacket& packet, std::string& err);
bool session_link_poll(std::vector<NetTransportPacket>& out, std::string& err);
const std::string& session_link_advertised_endpoint();
const std::string& session_link_last_error();
bool session_link_query_stats(SessionLinkStats& out);
