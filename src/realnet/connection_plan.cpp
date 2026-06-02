#include "gubsy/realnet/connection_plan.hpp"

#include "gubsy/lobby/connection_cascade.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace realnet {
namespace {

struct Ipv4Address {
    std::array<int, 4> octets{};
};

bool parse_ipv4_address(const std::string& address, Ipv4Address& out) {
    std::size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        const std::size_t dot = i == 3 ? std::string::npos : address.find('.', start);
        if (i < 3 && dot == std::string::npos)
            return false;
        const std::size_t end = i == 3 ? address.size() : dot;
        if (end <= start)
            return false;
        try {
            const std::string segment = address.substr(start, end - start);
            std::size_t parsed_chars = 0;
            const int octet = std::stoi(segment, &parsed_chars);
            if (parsed_chars != segment.size() || octet < 0 || octet > 255)
                return false;
            out.octets[static_cast<std::size_t>(i)] = octet;
        } catch (...) {
            return false;
        }
        start = end + 1;
    }
    return start == address.size() + 1;
}

bool is_private_ipv4(const Ipv4Address& address) {
    return address.octets[0] == 10 ||
           (address.octets[0] == 172 &&
            address.octets[1] >= 16 &&
            address.octets[1] <= 31) ||
           (address.octets[0] == 192 && address.octets[1] == 168);
}

bool is_loopback_ipv4(const Ipv4Address& address) {
    return address.octets[0] == 127;
}

std::uint32_t ipv4_to_u32(const Ipv4Address& address) {
    return (static_cast<std::uint32_t>(address.octets[0]) << 24U) |
           (static_cast<std::uint32_t>(address.octets[1]) << 16U) |
           (static_cast<std::uint32_t>(address.octets[2]) << 8U) |
           static_cast<std::uint32_t>(address.octets[3]);
}

int ipv4_prefix_from_mask(const sockaddr* netmask) {
    if (netmask == nullptr || netmask->sa_family != AF_INET)
        return -1;
    const auto* mask = reinterpret_cast<const sockaddr_in*>(netmask);
    const std::uint32_t value = ntohl(mask->sin_addr.s_addr);
    int prefix = 0;
    bool saw_zero = false;
    for (int bit = 31; bit >= 0; --bit) {
        const bool set = (value & (1U << static_cast<unsigned>(bit))) != 0;
        if (set && saw_zero)
            return -1;
        if (set)
            prefix += 1;
        else
            saw_zero = true;
    }
    return prefix;
}

int ipv6_prefix_from_mask(const sockaddr* netmask) {
    if (netmask == nullptr || netmask->sa_family != AF_INET6)
        return -1;
    const auto* mask = reinterpret_cast<const sockaddr_in6*>(netmask);
    int prefix = 0;
    bool saw_zero = false;
    for (unsigned char byte : mask->sin6_addr.s6_addr) {
        for (int bit = 7; bit >= 0; --bit) {
            const bool set = (byte & (1U << static_cast<unsigned>(bit))) != 0;
            if (set && saw_zero)
                return -1;
            if (set)
                prefix += 1;
            else
                saw_zero = true;
        }
    }
    return prefix;
}

std::uint32_t ipv4_prefix_mask(int prefix) {
    if (prefix <= 0)
        return 0;
    if (prefix >= 32)
        return 0xffffffffU;
    return 0xffffffffU << static_cast<unsigned>(32 - prefix);
}

bool same_ipv4_prefix(const Ipv4Address& a, const Ipv4Address& b, int prefix) {
    const int effective_prefix = prefix >= 0 ? prefix : 24;
    const std::uint32_t mask = ipv4_prefix_mask(effective_prefix);
    return (ipv4_to_u32(a) & mask) == (ipv4_to_u32(b) & mask);
}

std::string sockaddr_to_string(const sockaddr* address) {
    if (address == nullptr)
        return {};
    char buffer[INET6_ADDRSTRLEN]{};
    if (address->sa_family == AF_INET) {
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address);
        if (inet_ntop(AF_INET, &ipv4->sin_addr, buffer, sizeof(buffer)) != nullptr)
            return buffer;
    } else if (address->sa_family == AF_INET6) {
        const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address);
        if (inet_ntop(AF_INET6, &ipv6->sin6_addr, buffer, sizeof(buffer)) != nullptr)
            return buffer;
    }
    return {};
}

AddressScope classify_ipv6_scope(const std::string& host) {
    std::string lower;
    lower.reserve(host.size());
    for (char c : host)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (lower == "::1")
        return AddressScope::Loopback;
    if (lower.rfind("fe80:", 0) == 0)
        return AddressScope::LinkLocal;
    if (lower.rfind("fc", 0) == 0 || lower.rfind("fd", 0) == 0)
        return AddressScope::Private;
    if (lower.find(':') != std::string::npos)
        return AddressScope::Public;
    return AddressScope::Unknown;
}

bool direct_candidate_kind(ConnectionCandidateKind kind) {
    return kind == ConnectionCandidateKind::Loopback ||
           kind == ConnectionCandidateKind::LanDirect ||
           kind == ConnectionCandidateKind::PublicDirect;
}

AttemptTimelineEvent make_timeline_event(const PlannedConnectionCandidate& candidate) {
    return AttemptTimelineEvent{
        .event = candidate.decision == CandidateDecision::Try
                     ? "candidate_try"
                     : "candidate_skip",
        .candidate_kind = candidate.candidate.kind,
        .decision = candidate.decision,
        .phase = candidate.phase,
        .endpoint = candidate.candidate.endpoint,
        .reason = candidate.reason,
    };
}

bool local_private_ipv4_reachable(const std::string& host,
                                  const LocalNetworkInfo& local_network) {
    Ipv4Address host_address;
    if (!parse_ipv4_address(host, host_address))
        return false;
    for (const LocalInterfaceAddress& interface : local_network.interfaces) {
        Ipv4Address local_address;
        if (interface.family == AddressFamily::Ipv4 &&
            parse_ipv4_address(interface.address, local_address) &&
            same_ipv4_prefix(host_address, local_address, interface.prefix_length)) {
            return true;
        }
    }
    return false;
}

ConnectionCandidateKind forced_candidate_kind(const ConnectionPlanInput& input) {
    if (input.force_relay)
        return ConnectionCandidateKind::Relay;
    if (input.force_nat_punch)
        return ConnectionCandidateKind::NatPunch;
    return ConnectionCandidateKind::LanDirect;
}

PlannedConnectionCandidate plan_candidate(const ConnectionPlanInput& input,
                                          const ConnectionCandidate& candidate) {
    PlannedConnectionCandidate planned;
    planned.candidate = candidate;
    planned.phase = gubsy_connect_phase_for_candidate(candidate.kind);

    if (direct_candidate_kind(candidate.kind)) {
        if (!parse_endpoint(candidate.endpoint, planned.host, planned.port)) {
            planned.decision = CandidateDecision::SkipInvalidEndpoint;
            planned.reason = "Direct endpoint is missing or invalid.";
            return planned;
        }

        const AddressFamily family = classify_address_family(planned.host);
        const AddressScope scope = classify_address_scope(planned.host);
        if (scope == AddressScope::Private &&
            family == AddressFamily::Ipv4 &&
            !local_private_ipv4_reachable(planned.host, input.local_network)) {
            planned.decision = CandidateDecision::SkipUnreachablePrivate;
            planned.reason = "Private IPv4 endpoint does not appear local.";
            return planned;
        }
        if (scope == AddressScope::LinkLocal) {
            planned.decision = CandidateDecision::SkipUnsupported;
            planned.reason = "Link-local endpoints need scope handling.";
            return planned;
        }

        planned.decision = CandidateDecision::Try;
        planned.reason = "Direct endpoint is eligible.";
        return planned;
    }

    if (candidate.kind == ConnectionCandidateKind::NatPunch) {
        if (!input.nat_punch_supported || input.join_attempt_id.empty() ||
            input.punch_secret.empty()) {
            planned.decision = CandidateDecision::SkipDisabled;
            planned.reason = "NAT punch credentials or service capability are missing.";
            return planned;
        }
        planned.decision = CandidateDecision::Try;
        planned.reason = "NAT punch is available.";
        return planned;
    }

    if (candidate.kind == ConnectionCandidateKind::Relay) {
        planned.decision = input.relay_supported ? CandidateDecision::Try
                                                 : CandidateDecision::SkipDisabled;
        planned.reason = input.relay_supported ? "Relay is available."
                                               : "Relay service is not available.";
        return planned;
    }

    if (candidate.kind == ConnectionCandidateKind::Steam) {
        planned.decision = input.steam_supported ? CandidateDecision::Try
                                                 : CandidateDecision::SkipDisabled;
        planned.reason = input.steam_supported ? "Steam transport is available."
                                               : "Steam transport is not available.";
        return planned;
    }

    planned.decision = CandidateDecision::SkipUnsupported;
    planned.reason = "Candidate kind is unsupported.";
    return planned;
}

} // namespace

const char* address_family_id(AddressFamily family) {
    switch (family) {
        case AddressFamily::Unknown:
            return "unknown";
        case AddressFamily::Ipv4:
            return "ipv4";
        case AddressFamily::Ipv6:
            return "ipv6";
    }
    return "unknown";
}

const char* address_scope_id(AddressScope scope) {
    switch (scope) {
        case AddressScope::Unknown:
            return "unknown";
        case AddressScope::Loopback:
            return "loopback";
        case AddressScope::Private:
            return "private";
        case AddressScope::LinkLocal:
            return "link_local";
        case AddressScope::Public:
            return "public";
    }
    return "unknown";
}

const char* candidate_decision_id(CandidateDecision decision) {
    switch (decision) {
        case CandidateDecision::Try:
            return "try";
        case CandidateDecision::SkipInvalidEndpoint:
            return "skip_invalid_endpoint";
        case CandidateDecision::SkipUnreachablePrivate:
            return "skip_unreachable_private";
        case CandidateDecision::SkipUnsupported:
            return "skip_unsupported";
        case CandidateDecision::SkipDisabled:
            return "skip_disabled";
    }
    return "skip_unsupported";
}

bool parse_endpoint(const std::string& endpoint, std::string& host, std::uint16_t& port) {
    return gubsy_parse_endpoint(endpoint, host, port);
}

AddressFamily classify_address_family(const std::string& host) {
    Ipv4Address ipv4;
    if (parse_ipv4_address(host, ipv4))
        return AddressFamily::Ipv4;
    if (host.find(':') != std::string::npos)
        return AddressFamily::Ipv6;
    return AddressFamily::Unknown;
}

AddressScope classify_address_scope(const std::string& host) {
    Ipv4Address ipv4;
    if (parse_ipv4_address(host, ipv4)) {
        if (is_loopback_ipv4(ipv4))
            return AddressScope::Loopback;
        if (is_private_ipv4(ipv4))
            return AddressScope::Private;
        return AddressScope::Public;
    }
    return classify_ipv6_scope(host);
}

LocalNetworkInfo detect_local_network_info() {
    LocalNetworkInfo info;
#if defined(_WIN32)
    ULONG buffer_size = 15000;
    std::vector<unsigned char> buffer(buffer_size);
    IP_ADAPTER_ADDRESSES* addresses =
        reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    ULONG result = GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        nullptr,
        addresses,
        &buffer_size
    );
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buffer_size);
        addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        result = GetAdaptersAddresses(
            AF_UNSPEC,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr,
            addresses,
            &buffer_size
        );
    }
    if (result != NO_ERROR)
        return info;

    for (IP_ADAPTER_ADDRESSES* adapter = addresses; adapter != nullptr;
         adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;
        for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress;
             unicast != nullptr;
             unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr == nullptr)
                continue;
            const int family = unicast->Address.lpSockaddr->sa_family;
            if (family != AF_INET && family != AF_INET6)
                continue;
            LocalInterfaceAddress address;
            address.address = sockaddr_to_string(unicast->Address.lpSockaddr);
            address.family = family == AF_INET ? AddressFamily::Ipv4 : AddressFamily::Ipv6;
            address.scope = classify_address_scope(address.address);
            address.prefix_length = static_cast<int>(unicast->OnLinkPrefixLength);
            if (!address.address.empty())
                info.interfaces.push_back(std::move(address));
        }
    }
    return info;
#else
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0 || interfaces == nullptr)
        return info;
    for (ifaddrs* it = interfaces; it != nullptr; it = it->ifa_next) {
        if (it->ifa_addr == nullptr)
            continue;
        if ((it->ifa_flags & IFF_UP) == 0)
            continue;
        const int family = it->ifa_addr->sa_family;
        if (family != AF_INET && family != AF_INET6)
            continue;
        LocalInterfaceAddress address;
        address.address = sockaddr_to_string(it->ifa_addr);
        address.family = family == AF_INET ? AddressFamily::Ipv4 : AddressFamily::Ipv6;
        address.scope = classify_address_scope(address.address);
        address.prefix_length = family == AF_INET
                                    ? ipv4_prefix_from_mask(it->ifa_netmask)
                                    : ipv6_prefix_from_mask(it->ifa_netmask);
        if (!address.address.empty())
            info.interfaces.push_back(std::move(address));
    }
    freeifaddrs(interfaces);
    return info;
#endif
}

ConnectionPlan build_connection_plan(const ConnectionPlanInput& input) {
    ConnectionPlan plan;
    plan.room_code = input.room.room_code;
    plan.join_attempt_id = input.join_attempt_id;

    for (const ConnectionCandidate& candidate : gubsy_sorted_connection_candidates(input.room)) {
        plan.candidates.push_back(plan_candidate(input, candidate));
    }

    if (input.force_nat_punch || input.force_relay) {
        const ConnectionCandidateKind forced_kind = forced_candidate_kind(input);
        std::stable_sort(plan.candidates.begin(), plan.candidates.end(),
                         [forced_kind](const PlannedConnectionCandidate& a,
                                       const PlannedConnectionCandidate& b) {
                             if (a.candidate.kind == forced_kind &&
                                 b.candidate.kind != forced_kind) {
                                 return true;
                             }
                             if (b.candidate.kind == forced_kind &&
                                 a.candidate.kind != forced_kind) {
                                 return false;
                             }
                             return a.candidate.priority < b.candidate.priority;
                         });
    }
    plan.timeline.push_back(AttemptTimelineEvent{
        .event = "room_selected",
        .candidate_kind = ConnectionCandidateKind::LanDirect,
        .decision = CandidateDecision::Try,
        .phase = ConnectPhase::ResolvingRoom,
        .endpoint = {},
        .reason = input.room.room_code.empty()
                      ? "Room selected without a room code."
                      : "Room " + input.room.room_code + " selected.",
    });
    for (const PlannedConnectionCandidate& candidate : plan.candidates)
        plan.timeline.push_back(make_timeline_event(candidate));
    return plan;
}

} // namespace realnet
