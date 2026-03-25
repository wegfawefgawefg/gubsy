#include "utils.hpp"
#include <filesystem>
#include <system_error>

bool ensure_dir(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec;
}
