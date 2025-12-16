#include "data.hpp"

#include <filesystem>
#include <vector>

void ensure_data_folder_structure() {
    std::filesystem::path data_path(DATA_FOLDER_PATH);
    if (!std::filesystem::exists(data_path)) {
        std::filesystem::create_directory(data_path);
    }

    // list of subdirectories to create
    const std::vector<std::string> subdirs = {
        "binds_profiles",
        "player_profiles",
        "saves",
        "settings_profiles",
    };

    for (const auto& subdir : subdirs) {
        std::filesystem::path subdir_path = data_path / subdir;
        if (!std::filesystem::exists(subdir_path)) {
            std::filesystem::create_directory(subdir_path);
        }
    }



}