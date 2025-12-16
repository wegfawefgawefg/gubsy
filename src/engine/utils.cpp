#include "engine/utils.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

bool ensure_dir(const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (dir.empty()) return false;
    if (fs::exists(dir, ec)) return true;
    return fs::create_directories(dir, ec);
}