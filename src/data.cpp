#include "data.hpp"

#include "src/project_paths.hpp"

#include <filesystem>
#include <vector>

void ensure_data_folder_structure() {
    namespace fs = std::filesystem;

    fs::path root = data_root();
    std::error_code ec;
    fs::create_directories(root, ec);

    const std::vector<std::string> subdirs = {
        "binds_profiles",
        "cache",
        "logs",
        "mods",
        "player_profiles",
        "saves",
        "settings_profiles",
        "ui_layouts",
    };

    for (const auto& subdir : subdirs) {
        fs::create_directories(root / subdir, ec);
    }
}
