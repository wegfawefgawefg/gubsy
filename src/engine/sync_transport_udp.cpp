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

#include "engine/net_transport.hpp"

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

UdpJsonNetTransport::~UdpJsonNetTransport() {
    reset();
}

void UdpJsonNetTransport::reset() {
    if (socket_fd_ >= 0)
        close(socket_fd_);
    open_ = false;
    is_host_ = false;
    socket_fd_ = -1;
    room_code_.clear();
    remote_endpoint_.clear();
    public_endpoint_.clear();
    member_endpoints_.clear();
}

bool UdpJsonNetTransport::open_socket(bool is_host,
                                      const std::string& room_code,
                                      const std::string& remote_endpoint,
                                      std::string& err) {
    reset();

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        err = std::strerror(errno);
        return false;
    }

    if (!bind_udp_socket(socket_fd, 0, err) || !set_nonblocking(socket_fd, err)) {
        close(socket_fd);
        return false;
    }

    open_ = true;
    is_host_ = is_host;
    socket_fd_ = socket_fd;
    room_code_ = room_code;
    remote_endpoint_ = remote_endpoint;
    member_endpoints_.clear();

    sockaddr_in local_addr{};
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&local_addr), &local_len) == 0) {
        public_endpoint_ =
            "udp://" + public_host_name() + ":" + std::to_string(ntohs(local_addr.sin_port));
    }
    return true;
}

bool UdpJsonNetTransport::ensure_host(const std::string& room_code, std::string& err) {
    if (same_mode(open_, is_host_, room_code_, remote_endpoint_, true, room_code, ""))
        return true;
    return open_socket(true, room_code, "", err);
}

bool UdpJsonNetTransport::ensure_client(const std::string& room_code,
                                        const std::string& remote_endpoint,
                                        std::string& err) {
    if (same_mode(open_, is_host_, room_code_, remote_endpoint_, false, room_code, remote_endpoint))
        return true;

    ParsedUdpEndpoint parsed{};
    if (!parse_udp_endpoint(remote_endpoint, parsed, err))
        return false;
    return open_socket(false, room_code, remote_endpoint, err);
}

bool UdpJsonNetTransport::send(const NetTransportPacket& packet, std::string& err) {
    if (!open_) {
        err = "realtime transport is not open";
        return false;
    }

    if (!is_host_) {
        ParsedUdpEndpoint remote{};
        if (!parse_udp_endpoint(remote_endpoint_, remote, err))
            return false;
        nlohmann::json packet_json{
            {"type", packet.kind == NetPacketKind::Snapshot ? "snapshot" : "input"},
            {"room_code", room_code_},
            {"member_id", packet.member_id},
            {"seq", packet.seq},
            {"payload", packet.payload},
        };
        return send_json_packet(socket_fd_, remote, packet_json, err);
    }

    nlohmann::json packet_json{
        {"type", packet.kind == NetPacketKind::Snapshot ? "snapshot" : "input"},
        {"room_code", room_code_},
        {"member_id", packet.member_id},
        {"seq", packet.seq},
        {"payload", packet.payload},
    };
    for (const auto& entry : member_endpoints_) {
        ParsedUdpEndpoint endpoint{};
        if (!parse_udp_endpoint("udp://" + entry.second, endpoint, err))
            return false;
        if (!send_json_packet(socket_fd_, endpoint, packet_json, err))
            return false;
    }
    return true;
}

bool UdpJsonNetTransport::poll(std::vector<NetTransportPacket>& out, std::string& err) {
    out.clear();
    if (!open_) {
        err = "realtime transport is not open";
        return false;
    }

    std::vector<NetTransportPacket> latest_inputs;
    bool has_snapshot = false;
    NetTransportPacket latest_snapshot;

    for (;;) {
        nlohmann::json packet;
        std::string sender_endpoint;
        bool had_packet = false;
        if (!drain_packet(socket_fd_, packet, sender_endpoint, had_packet, err))
            return false;
        if (!had_packet)
            break;
        if (!packet.is_object() ||
            packet.value("room_code", std::string{}) != room_code_) {
            continue;
        }

        const std::string type = packet.value("type", std::string{});
        NetTransportPacket entry;
        entry.room_code = room_code_;
        entry.member_id = packet.value("member_id", "");
        entry.seq = packet.value("seq", std::uint64_t{0});
        auto payload_it = packet.find("payload");
        if (payload_it != packet.end() && payload_it->is_object())
            entry.payload = *payload_it;

        if (type == "input") {
            if (!is_host_ || entry.member_id.empty())
                continue;
            entry.kind = NetPacketKind::Input;
            remember_member_endpoint(member_endpoints_, entry.member_id, sender_endpoint);
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

        if (type == "snapshot") {
            entry.kind = NetPacketKind::Snapshot;
            latest_snapshot = std::move(entry);
            has_snapshot = true;
        }
    }

    if (is_host_) {
        out = std::move(latest_inputs);
        return true;
    }

    if (has_snapshot)
        out.push_back(std::move(latest_snapshot));
    return true;
}

const std::string& UdpJsonNetTransport::public_endpoint() const {
    return public_endpoint_;
}
