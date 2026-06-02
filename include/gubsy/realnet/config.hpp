#pragma once

#include <cstdint>

namespace realnet {

struct PunchTimingConfig {
    std::uint64_t hello_interval_ms{250};
    std::uint64_t probe_interval_ms{50};
    std::uint64_t punch_window_ms{3000};
};

struct Config {
    PunchTimingConfig punch;
};

const Config& default_config();

} // namespace realnet
