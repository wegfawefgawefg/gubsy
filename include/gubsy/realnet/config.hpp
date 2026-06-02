#pragma once

#include <cstdint>

namespace realnet {

struct PunchTimingConfig {
    std::uint64_t hello_interval_ms{250};
    std::uint64_t probe_interval_ms{50};
    std::uint64_t punch_window_ms{3000};
};

struct RelayTimingConfig {
    std::uint64_t hello_interval_ms{250};
    std::uint64_t keepalive_interval_ms{1000};
    std::uint64_t setup_timeout_ms{3000};
    std::uint64_t allocation_ttl_ms{30000};
    std::uint64_t idle_timeout_ms{10000};
};

struct RelayServiceConfig {
    std::uint64_t max_packet_bytes{1400};
    RelayTimingConfig timing;
};

struct Config {
    PunchTimingConfig punch;
    RelayServiceConfig relay;
};

const Config& default_config();

} // namespace realnet
