#include "gubsy/realnet/config.hpp"

namespace realnet {

const Config& default_config() {
    static const Config config{};
    return config;
}

} // namespace realnet
