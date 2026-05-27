#include "src/sync_transport_udp.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "src/net_transport.hpp"
#include "src/sync_transport_packet_codec.hpp"

namespace {

constexpr std::size_t kMaxPacketBytes = 60 * 1024;
constexpr std::uintptr_t kInvalidSocket = ~std::uintptr_t{0};

#ifdef _WIN32
using NativeSocket = SOCKET;
using SocketResult = int;
using SendRecvSize = int;
using SockLen = int;

NativeSocket native_socket(std::uintptr_t handle) {
    return static_cast<NativeSocket>(handle);
}

bool ensure_winsock(std::string& err) {
    static const int startup_result = []() {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    if (startup_result != 0) {
        err = "WSAStartup: winsock error " + std::to_string(startup_result);
        return false;
    }
    return true;
}

std::string socket_error_text() {
    return "winsock error " + std::to_string(WSAGetLastError());
}

bool would_block() {
    return WSAGetLastError() == WSAEWOULDBLOCK;
}

void close_socket(NativeSocket socket_fd) {
    closesocket(socket_fd);
}
#else
using NativeSocket = int;
using SocketResult = ssize_t;
using SendRecvSize = std::size_t;
using SockLen = socklen_t;

NativeSocket native_socket(std::uintptr_t handle) {
    return static_cast<NativeSocket>(handle);
}

bool ensure_winsock(std::string&) {
    return true;
}

std::string socket_error_text() {
    return std::strerror(errno);
}

bool would_block() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

void close_socket(NativeSocket socket_fd) {
    close(socket_fd);
}
#endif

struct ParsedUdpEndpoint {
    std::string host;
    std::uint16_t port{0};
};

bool parse_udp_endpoint(const std::string& endpoint, ParsedUdpEndpoint& out, std::string& err) {
    if (endpoint.rfind("udp://", 0) != 0) {
        err = "Only udp:// realtime endpoints are supported";
        return false;
    }

    std::string work = endpoint.substr(6);
    auto colon = work.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= work.size()) {
        err = "Invalid realtime endpoint";
        return false;
    }

    out.host = work.substr(0, colon);
    try {
        int port = std::stoi(work.substr(colon + 1));
        if (port <= 0 || port > 65535) {
            err = "Realtime endpoint port out of range";
            return false;
        }
        out.port = static_cast<std::uint16_t>(port);
    } catch (...) {
        err = "Invalid realtime endpoint port";
        return false;
    }
    return !out.host.empty();
}

bool set_nonblocking(NativeSocket socket_fd, std::string& err) {
#ifdef _WIN32
    u_long nonblocking = 1;
    const long nonblocking_command = static_cast<long>(FIONBIO);
    if (ioctlsocket(socket_fd, nonblocking_command, &nonblocking) != 0) {
        err = socket_error_text();
        return false;
    }
    return true;
#else
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        err = socket_error_text();
        return false;
    }
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        err = socket_error_text();
        return false;
    }
    return true;
#endif
}

bool bind_udp_socket(NativeSocket socket_fd, std::uint16_t port, std::string& err) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        err = socket_error_text();
        return false;
    }
    return true;
}

bool endpoint_to_sockaddr(const ParsedUdpEndpoint& endpoint, sockaddr_in& out, std::string& err) {
    out = {};
    out.sin_family = AF_INET;
    out.sin_port = htons(endpoint.port);
    if (inet_pton(AF_INET, endpoint.host.c_str(), &out.sin_addr) != 1) {
        err = "Only IPv4 realtime endpoints are supported";
        return false;
    }
    return true;
}

std::string public_host_name() {
    if (const char* value = std::getenv("GUB_SYNC_PUBLIC_HOST")) {
        if (*value != '\0')
            return value;
    }
    return "127.0.0.1";
}

std::string sender_endpoint_string(const sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)))
        return {};
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

bool send_packet_bytes(std::uintptr_t socket_fd,
                       const ParsedUdpEndpoint& endpoint,
                       const std::vector<std::uint8_t>& packet,
                       std::string& err) {
    sockaddr_in addr{};
    if (!endpoint_to_sockaddr(endpoint, addr, err))
        return false;
    SocketResult sent = sendto(native_socket(socket_fd),
                               reinterpret_cast<const char*>(packet.data()),
                               static_cast<SendRecvSize>(packet.size()),
                               0,
                               reinterpret_cast<const sockaddr*>(&addr),
                               sizeof(addr));
    if (sent < 0 || static_cast<std::size_t>(sent) != packet.size()) {
        err = socket_error_text();
        return false;
    }
    return true;
}

bool drain_packet(std::uintptr_t socket_fd,
                  std::vector<std::uint8_t>& packet_out,
                  std::string& sender_endpoint,
                  bool& had_packet,
                  std::string& err) {
    had_packet = false;
    std::vector<char> buffer(kMaxPacketBytes);
    sockaddr_in sender{};
    SockLen sender_len = sizeof(sender);
    SocketResult recv_len = recvfrom(native_socket(socket_fd),
                                     buffer.data(),
                                     static_cast<SendRecvSize>(buffer.size()),
                                     0,
                                     reinterpret_cast<sockaddr*>(&sender),
                                     &sender_len);
    if (recv_len < 0) {
        if (would_block())
            return true;
        err = socket_error_text();
        return false;
    }

    had_packet = true;
    sender_endpoint = sender_endpoint_string(sender);
    packet_out.assign(buffer.begin(), buffer.begin() + recv_len);
    return true;
}

std::uint64_t monotonic_ms() {
    using Clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count());
}

int env_int(const char* name, int fallback, int min_value = 0, int max_value = 100000) {
    const char* value = std::getenv(name);
    if (!value || *value == '\0')
        return fallback;
    try {
        int parsed = std::stoi(value);
        return std::clamp(parsed, min_value, max_value);
    } catch (...) {
        return fallback;
    }
}

bool same_mode(bool open,
               bool is_host_active,
               const std::string& active_room_code,
               const std::string& active_remote_endpoint,
               bool is_host,
               const std::string& room_code,
               const std::string& remote_endpoint) {
    return open &&
           is_host_active == is_host &&
           active_room_code == room_code &&
           active_remote_endpoint == remote_endpoint;
}

void remember_member_endpoint(std::vector<std::pair<std::string, std::string>>& endpoints,
                              const std::string& member_id,
                              const std::string& endpoint) {
    for (auto& entry : endpoints) {
        if (entry.first == member_id) {
            entry.second = endpoint;
            return;
        }
    }
    endpoints.push_back({member_id, endpoint});
}

} // namespace

UdpSyncNetTransport::~UdpSyncNetTransport() {
    reset();
}

void UdpSyncNetTransport::reset() {
    if (socket_fd_ != kInvalidSocket)
        close_socket(native_socket(socket_fd_));
    open_ = false;
    is_host_ = false;
    socket_fd_ = kInvalidSocket;
    room_code_.clear();
    remote_endpoint_.clear();
    public_endpoint_.clear();
    member_endpoints_.clear();
    pending_packets_.clear();
}

void UdpSyncNetTransport::load_simulation_config() {
    simulated_latency_ms_ = env_int("GUB_SYNC_SIMULATED_LATENCY_MS", 0, 0, 30000);
    simulated_jitter_ms_ = env_int("GUB_SYNC_SIMULATED_JITTER_MS", 0, 0, 30000);
    simulated_drop_pct_ = env_int("GUB_SYNC_SIMULATED_DROP_PCT", 0, 0, 100);
}

bool UdpSyncNetTransport::open_socket(bool is_host,
                                      const std::string& room_code,
                                      const std::string& remote_endpoint,
                                      std::string& err) {
    reset();
    load_simulation_config();

    if (!ensure_winsock(err))
        return false;

    NativeSocket socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == native_socket(kInvalidSocket)) {
        err = socket_error_text();
        return false;
    }

    if (!bind_udp_socket(socket_fd, 0, err) || !set_nonblocking(socket_fd, err)) {
        close_socket(socket_fd);
        return false;
    }

    open_ = true;
    is_host_ = is_host;
    socket_fd_ = static_cast<std::uintptr_t>(socket_fd);
    room_code_ = room_code;
    remote_endpoint_ = remote_endpoint;
    member_endpoints_.clear();

    sockaddr_in local_addr{};
    SockLen local_len = sizeof(local_addr);
    if (getsockname(native_socket(socket_fd_), reinterpret_cast<sockaddr*>(&local_addr), &local_len) == 0) {
        public_endpoint_ =
            "udp://" + public_host_name() + ":" + std::to_string(ntohs(local_addr.sin_port));
    }
    return true;
}

bool UdpSyncNetTransport::ensure_host(const std::string& room_code, std::string& err) {
    if (same_mode(open_, is_host_, room_code_, remote_endpoint_, true, room_code, ""))
        return true;
    return open_socket(true, room_code, "", err);
}

bool UdpSyncNetTransport::ensure_client(const std::string& room_code,
                                        const std::string& remote_endpoint,
                                        std::string& err) {
    if (same_mode(open_, is_host_, room_code_, remote_endpoint_, false, room_code, remote_endpoint))
        return true;

    ParsedUdpEndpoint parsed{};
    if (!parse_udp_endpoint(remote_endpoint, parsed, err))
        return false;
    return open_socket(false, room_code, remote_endpoint, err);
}

bool UdpSyncNetTransport::send(const NetTransportPacket& packet, std::string& err) {
    if (!open_) {
        err = "realtime transport is not open";
        return false;
    }

    std::vector<std::uint8_t> packet_bytes;
    if (!sync_transport_packet_serialize(packet, packet_bytes, err))
        return false;

    if (!is_host_) {
        ParsedUdpEndpoint remote{};
        if (!parse_udp_endpoint(remote_endpoint_, remote, err))
            return false;
        return send_packet_bytes(socket_fd_, remote, packet_bytes, err);
    }

    for (const auto& entry : member_endpoints_) {
        ParsedUdpEndpoint endpoint{};
        if (!parse_udp_endpoint("udp://" + entry.second, endpoint, err))
            return false;
        if (!send_packet_bytes(socket_fd_, endpoint, packet_bytes, err))
            return false;
    }
    return true;
}

namespace {

std::uint32_t next_random(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

bool should_drop_packet(int drop_pct, std::uint32_t& state) {
    if (drop_pct <= 0)
        return false;
    return static_cast<int>(next_random(state) % 100u) < drop_pct;
}

std::uint64_t packet_release_time_ms(int latency_ms,
                                     int jitter_ms,
                                     std::uint32_t& state) {
    int release_offset = latency_ms;
    if (jitter_ms > 0) {
        const int span = jitter_ms * 2 + 1;
        release_offset += static_cast<int>(next_random(state) %
                                           static_cast<std::uint32_t>(span)) -
                          jitter_ms;
    }
    return monotonic_ms() + static_cast<std::uint64_t>(std::max(0, release_offset));
}

} // namespace

bool UdpSyncNetTransport::poll(std::vector<NetTransportPacket>& out, std::string& err) {
    out.clear();
    if (!open_) {
        err = "realtime transport is not open";
        return false;
    }

    std::vector<NetTransportPacket> latest_inputs;
    bool has_snapshot = false;
    NetTransportPacket latest_snapshot;

    for (;;) {
        std::vector<std::uint8_t> packet;
        std::string sender_endpoint;
        bool had_packet = false;
        if (!drain_packet(socket_fd_, packet, sender_endpoint, had_packet, err))
            return false;
        if (!had_packet)
            break;
        if (should_drop_packet(simulated_drop_pct_, random_state_))
            continue;
        PendingPacket pending;
        pending.packet = std::move(packet);
        pending.sender_endpoint = std::move(sender_endpoint);
        pending.release_at_ms =
            packet_release_time_ms(simulated_latency_ms_, simulated_jitter_ms_, random_state_);
        pending_packets_.push_back(std::move(pending));
    }

    const std::uint64_t now_ms = monotonic_ms();
    std::vector<PendingPacket> still_pending;
    still_pending.reserve(pending_packets_.size());
    for (auto& pending : pending_packets_) {
        if (pending.release_at_ms > now_ms) {
            still_pending.push_back(std::move(pending));
            continue;
        }

        NetTransportPacket entry;
        if (!sync_transport_packet_deserialize(pending.packet, entry, err))
            return false;
        if (entry.room_code != room_code_) {
            continue;
        }

        if (entry.kind == NetPacketKind::Input) {
            if (!is_host_ || entry.member_id.empty())
                continue;
            remember_member_endpoint(member_endpoints_, entry.member_id, pending.sender_endpoint);
            bool replaced = false;
            for (auto& latest : latest_inputs) {
                if (latest.member_id == entry.member_id) {
                    latest = std::move(entry);
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
                latest_inputs.push_back(std::move(entry));
            continue;
        }

        if (entry.kind == NetPacketKind::Snapshot) {
            latest_snapshot = std::move(entry);
            has_snapshot = true;
        }
    }
    pending_packets_ = std::move(still_pending);

    if (is_host_) {
        out = std::move(latest_inputs);
        return true;
    }

    if (has_snapshot)
        out.push_back(std::move(latest_snapshot));
    return true;
}

const std::string& UdpSyncNetTransport::public_endpoint() const {
    return public_endpoint_;
}
