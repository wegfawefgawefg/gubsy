#include "engine/session_link.hpp"

#include <algorithm>
#include <utility>

#include "engine/sync_session_wire.hpp"
#include "engine/sync_transport_udp.hpp"

namespace {

struct SessionLinkRuntime {
    SessionLinkHooks hooks{};
    bool configured{false};
    bool active{false};
    bool is_host{false};
    bool transport_ready{false};
    std::uint64_t generation{0};
    std::string server_url;
    std::string room_code;
    std::string host_secret;
    std::string local_member_id;
    SessionContract contract{};
    std::string last_error;
    double connected_at{0.0};
    double last_packet_received_at{0.0};
    UdpSyncNetTransport transport{};
};

SessionLinkRuntime g_link;

double runtime_now() {
    if (g_link.hooks.query_now)
        return g_link.hooks.query_now(g_link.hooks.ctx);
    return 0.0;
}

bool connection_changed(const SessionLinkConnection& connection) {
    return !g_link.active ||
           g_link.is_host != connection.is_host ||
           g_link.server_url != connection.server_url ||
           g_link.room_code != sync_session_normalized_room_code(connection.room_code) ||
           g_link.host_secret != connection.host_secret ||
           g_link.local_member_id != connection.local_member_id ||
           !session_contract_equal(g_link.contract, connection.contract);
}

void start_connection(const SessionLinkConnection& connection) {
    SessionLinkRuntime next = g_link;
    next.active = true;
    next.is_host = connection.is_host;
    next.transport_ready = false;
    next.generation += 1;
    next.server_url = connection.server_url;
    next.room_code = sync_session_normalized_room_code(connection.room_code);
    next.host_secret = connection.host_secret;
    next.local_member_id = connection.local_member_id;
    next.contract = connection.contract;
    next.last_error.clear();
    next.connected_at = runtime_now();
    next.last_packet_received_at = next.connected_at;
    next.transport.reset();
    g_link = std::move(next);
}

bool ensure_transport_ready(std::string& err) {
    if (!g_link.active)
        return false;
    if (g_link.is_host) {
        g_link.transport_ready = g_link.transport.ensure_host(g_link.room_code, err);
        return g_link.transport_ready;
    }
    if (g_link.contract.realtime_endpoint.empty()) {
        err = "Waiting for host realtime endpoint";
        g_link.transport_ready = false;
        return false;
    }
    g_link.transport_ready =
        g_link.transport.ensure_client(g_link.room_code, g_link.contract.realtime_endpoint, err);
    return g_link.transport_ready;
}

} // namespace

void session_link_configure(const SessionLinkHooks& hooks) {
    g_link.hooks = hooks;
    g_link.configured = true;
}

void session_link_reset() {
    g_link.active = false;
    g_link.is_host = false;
    g_link.transport_ready = false;
    g_link.server_url.clear();
    g_link.room_code.clear();
    g_link.host_secret.clear();
    g_link.local_member_id.clear();
    g_link.contract = SessionContract{};
    g_link.contract.net_protocol = session_contract_default_net_protocol();
    g_link.last_error.clear();
    g_link.connected_at = 0.0;
    g_link.last_packet_received_at = 0.0;
    g_link.transport.reset();
}

bool session_link_active() {
    return g_link.active;
}

bool session_link_update(std::string& err) {
    err.clear();
    if (!g_link.configured || !g_link.hooks.query_connection)
        return false;

    SessionLinkConnection connection;
    if (!g_link.hooks.query_connection(g_link.hooks.ctx, connection) || !connection.active) {
        session_link_reset();
        return false;
    }
    if (connection_changed(connection))
        start_connection(connection);
    if (g_link.hooks.tick_presence)
        g_link.hooks.tick_presence(g_link.hooks.ctx);
    if (!ensure_transport_ready(err)) {
        if (!err.empty())
            g_link.last_error = err;
        return false;
    }
    g_link.last_error.clear();
    return true;
}

bool session_link_query_connection(SessionLinkConnection& out) {
    if (!g_link.active)
        return false;
    out.active = g_link.active;
    out.is_host = g_link.is_host;
    out.server_url = g_link.server_url;
    out.room_code = g_link.room_code;
    out.host_secret = g_link.host_secret;
    out.local_member_id = g_link.local_member_id;
    out.contract = g_link.contract;
    return true;
}

bool session_link_send(const NetTransportPacket& packet, std::string& err) {
    if (!g_link.transport_ready) {
        err = "Realtime session link is not ready";
        return false;
    }
    return g_link.transport.send(packet, err);
}

bool session_link_poll(std::vector<NetTransportPacket>& out, std::string& err) {
    out.clear();
    if (!g_link.transport_ready) {
        err = "Realtime session link is not ready";
        return false;
    }
    if (!g_link.transport.poll(out, err))
        return false;
    if (!out.empty())
        g_link.last_packet_received_at = runtime_now();
    return true;
}

const std::string& session_link_advertised_endpoint() {
    return g_link.transport.public_endpoint();
}

const std::string& session_link_last_error() {
    return g_link.last_error;
}

bool session_link_query_stats(SessionLinkStats& out) {
    out = SessionLinkStats{};
    if (!g_link.active)
        return false;
    const double now = runtime_now();
    out.active = g_link.active;
    out.is_host = g_link.is_host;
    out.transport_ready = g_link.transport_ready;
    out.generation = g_link.generation;
    out.connected_for_sec = std::max(0.0, now - g_link.connected_at);
    out.packet_idle_sec = std::max(0.0, now - g_link.last_packet_received_at);
    return true;
}
