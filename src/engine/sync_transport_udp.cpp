#include "engine/sync_transport_udp.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr std::size_t kMaxPacketBytes = 60 * 1024;

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

bool set_nonblocking(int socket_fd, std::string& err) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        err = std::strerror(errno);
        return false;
    }
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        err = std::strerror(errno);
        return false;
    }
    return true;
}

bool bind_udp_socket(int socket_fd, std::uint16_t port, std::string& err) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        err = std::strerror(errno);
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

bool send_json_packet(int socket_fd,
                      const ParsedUdpEndpoint& endpoint,
                      const nlohmann::json& packet,
                      std::string& err) {
    sockaddr_in addr{};
    if (!endpoint_to_sockaddr(endpoint, addr, err))
        return false;
    std::string payload = packet.dump();
    ssize_t sent = sendto(socket_fd,
                          payload.data(),
                          payload.size(),
                          0,
                          reinterpret_cast<const sockaddr*>(&addr),
                          sizeof(addr));
    if (sent < 0 || static_cast<std::size_t>(sent) != payload.size()) {
        err = std::strerror(errno);
        return false;
    }
    return true;
}

bool drain_packet(int socket_fd,
                  nlohmann::json& packet_out,
                  std::string& sender_endpoint,
                  bool& had_packet,
                  std::string& err) {
    had_packet = false;
    std::vector<char> buffer(kMaxPacketBytes);
    sockaddr_in sender{};
    socklen_t sender_len = sizeof(sender);
    ssize_t recv_len = recvfrom(socket_fd,
                                buffer.data(),
                                buffer.size(),
                                0,
                                reinterpret_cast<sockaddr*>(&sender),
                                &sender_len);
    if (recv_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
        err = std::strerror(errno);
        return false;
    }

    had_packet = true;
    sender_endpoint = sender_endpoint_string(sender);
    try {
        packet_out = nlohmann::json::parse(buffer.data(), buffer.data() + recv_len);
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
    return true;
}

bool same_mode(const SyncUdpTransport& transport,
               bool is_host,
               const std::string& room_code,
               const std::string& remote_endpoint) {
    return transport.open &&
           transport.is_host == is_host &&
           transport.room_code == room_code &&
           transport.remote_endpoint == remote_endpoint;
}

bool open_transport(SyncUdpTransport& transport,
                    bool is_host,
                    const std::string& room_code,
                    const std::string& remote_endpoint,
                    std::string& err) {
    sync_udp_transport_reset(transport);

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        err = std::strerror(errno);
        return false;
    }

    if (!bind_udp_socket(socket_fd, 0, err) || !set_nonblocking(socket_fd, err)) {
        close(socket_fd);
        return false;
    }

    transport.open = true;
    transport.is_host = is_host;
    transport.socket_fd = socket_fd;
    transport.room_code = room_code;
    transport.remote_endpoint = remote_endpoint;
    transport.member_endpoints.clear();

    sockaddr_in local_addr{};
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&local_addr), &local_len) == 0) {
        transport.public_endpoint =
            "udp://" + public_host_name() + ":" + std::to_string(ntohs(local_addr.sin_port));
    }
    return true;
}

void remember_member_endpoint(SyncUdpTransport& transport,
                              const std::string& member_id,
                              const std::string& endpoint) {
    for (auto& entry : transport.member_endpoints) {
        if (entry.first == member_id) {
            entry.second = endpoint;
            return;
        }
    }
    transport.member_endpoints.push_back({member_id, endpoint});
}

} // namespace

void sync_udp_transport_reset(SyncUdpTransport& transport) {
    if (transport.socket_fd >= 0)
        close(transport.socket_fd);
    transport = SyncUdpTransport{};
}

bool sync_udp_transport_ensure_host(SyncUdpTransport& transport,
                                    const std::string& room_code,
                                    std::string& err) {
    if (same_mode(transport, true, room_code, ""))
        return true;
    return open_transport(transport, true, room_code, "", err);
}

bool sync_udp_transport_ensure_client(SyncUdpTransport& transport,
                                      const std::string& room_code,
                                      const std::string& remote_endpoint,
                                      std::string& err) {
    if (same_mode(transport, false, room_code, remote_endpoint))
        return true;

    ParsedUdpEndpoint parsed{};
    if (!parse_udp_endpoint(remote_endpoint, parsed, err))
        return false;
    return open_transport(transport, false, room_code, remote_endpoint, err);
}

bool sync_udp_transport_send_input(SyncUdpTransport& transport,
                                   const std::string& member_id,
                                   const SequencedInput& input,
                                   std::string& err) {
    if (!transport.open || transport.is_host) {
        err = "client realtime transport is not open";
        return false;
    }

    ParsedUdpEndpoint remote{};
    if (!parse_udp_endpoint(transport.remote_endpoint, remote, err))
        return false;

    nlohmann::json packet{
        {"type", "input"},
        {"room_code", transport.room_code},
        {"member_id", member_id},
        {"seq", input.seq},
        {"payload", input.payload},
    };
    return send_json_packet(transport.socket_fd, remote, packet, err);
}

bool sync_udp_transport_collect_host_inputs(SyncUdpTransport& transport,
                                            std::vector<SyncTransportMemberInput>& out,
                                            std::string& err) {
    out.clear();
    if (!transport.open || !transport.is_host) {
        err = "host realtime transport is not open";
        return false;
    }

    std::vector<SyncTransportMemberInput> latest;
    for (;;) {
        nlohmann::json packet;
        std::string sender_endpoint;
        bool had_packet = false;
        if (!drain_packet(transport.socket_fd, packet, sender_endpoint, had_packet, err))
            return false;
        if (!had_packet)
            break;
        if (!packet.is_object() ||
            packet.value("type", std::string{}) != "input" ||
            packet.value("room_code", std::string{}) != transport.room_code) {
            continue;
        }

        const std::string member_id = packet.value("member_id", "");
        if (member_id.empty())
            continue;

        SequencedInput input;
        input.seq = packet.value("seq", std::uint64_t{0});
        auto payload_it = packet.find("payload");
        if (payload_it != packet.end() && payload_it->is_object())
            input.payload = *payload_it;
        remember_member_endpoint(transport, member_id, sender_endpoint);

        bool replaced = false;
        for (auto& entry : latest) {
            if (entry.member_id == member_id) {
                entry.input = std::move(input);
                replaced = true;
                break;
            }
        }
        if (!replaced)
            latest.push_back({member_id, std::move(input)});
    }

    out = std::move(latest);
    return true;
}

bool sync_udp_transport_send_snapshot(SyncUdpTransport& transport,
                                      const nlohmann::json& snapshot,
                                      std::string& err) {
    if (!transport.open || !transport.is_host) {
        err = "host realtime transport is not open";
        return false;
    }

    const nlohmann::json packet{
        {"type", "snapshot"},
        {"room_code", transport.room_code},
        {"snapshot", snapshot},
    };
    for (const auto& entry : transport.member_endpoints) {
        ParsedUdpEndpoint endpoint{};
        if (!parse_udp_endpoint("udp://" + entry.second, endpoint, err))
            return false;
        if (!send_json_packet(transport.socket_fd, endpoint, packet, err))
            return false;
    }
    return true;
}

bool sync_udp_transport_collect_client_snapshot(SyncUdpTransport& transport,
                                                nlohmann::json& snapshot_out,
                                                bool& has_snapshot,
                                                std::string& err) {
    snapshot_out = nlohmann::json::object();
    has_snapshot = false;
    if (!transport.open || transport.is_host) {
        err = "client realtime transport is not open";
        return false;
    }

    for (;;) {
        nlohmann::json packet;
        std::string sender_endpoint;
        bool had_packet = false;
        if (!drain_packet(transport.socket_fd, packet, sender_endpoint, had_packet, err))
            return false;
        if (!had_packet)
            break;
        if (!packet.is_object() ||
            packet.value("type", std::string{}) != "snapshot" ||
            packet.value("room_code", std::string{}) != transport.room_code) {
            continue;
        }

        auto snapshot_it = packet.find("snapshot");
        if (snapshot_it != packet.end() && snapshot_it->is_object()) {
            snapshot_out = *snapshot_it;
            has_snapshot = true;
        }
    }
    return true;
}

const std::string& sync_udp_transport_public_endpoint(const SyncUdpTransport& transport) {
    return transport.public_endpoint;
}
