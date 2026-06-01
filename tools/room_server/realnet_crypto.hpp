#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace realnet {

std::vector<std::uint8_t> hmac_sha256(const std::string& key, const std::string& message);
std::string hmac_sha256_hex(const std::string& key, const std::string& message,
                            std::size_t truncate_bytes = 32);
bool constant_time_equal(const std::string& a, const std::string& b);

} // namespace realnet
