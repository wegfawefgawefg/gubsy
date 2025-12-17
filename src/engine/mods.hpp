#pragma once

#include "engine/sprites.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct ModFileEntry {
    std::string path;
    std::uint64_t size_bytes{0};
};

struct ModCatalogEntry {
    std::string id;
    std::string folder;
    std::string title;
    std::string author;
    std::string version;
    std::string summary;
    std::string description;
    std::vector<std::string> dependencies;
    std::string game_version;
    std::vector<std::string> apis;
    std::vector<ModFileEntry> files;
    std::uint64_t total_bytes{0};
    bool required{false};
    bool installed{false};
    bool installing{false};
    bool uninstalling{false};
    std::string status_text;
};

struct LobbyModEntry {
    std::string id;
    std::string title;
    std::string author;
    std::string description;
    std::vector<std::string> dependencies;
    bool required{false};
    bool enabled{false};
};
struct ModInfo {
    std::string name;
    std::string title;
    std::string version;
    std::vector<std::string> deps;
    std::vector<std::string> optional_deps;
    std::vector<std::string> apis;
    std::string game_version;
    std::string path; // absolute or relative root
    std::string manifest_path;
    std::string description;
    std::string download_url;
    std::string author;
    bool enabled{true};
    bool required{false};
};

// Very small mod manager that discovers mods in `mods/`,
// loads `info.toml`, and supports polling-based hot reload
// for `graphics/` and `scripts/` folders.
struct ModManager {
    using Clock = std::chrono::steady_clock;
    std::string root;
    std::vector<ModInfo> mods;
    std::unordered_map<std::string, std::filesystem::file_time_type> tracked_files;
    bool registry_built = false;
    float accum_poll = 0.0; // seconds
   
    
    
    
    
    // helpers
    static ModInfo parse_info(const std::string& mod_path);
    void track_tree(const std::string& path);
    bool check_changes(std::vector<std::string>& changed_assets,
        std::vector<std::string>& changed_scripts);
};

// Initialize global Mods manager instance. Returns true on success.
bool init_mods_manager(const std::string& mods_root = "mods");

// Discover available mods (non-recursive: `mods/*/`).
void discover_mods();

// Poll filesystem for changes and trigger rebuilds as needed.
// Returns true if anything reloaded.
bool poll_fs_mods_hot_reload();    

// Build a sprite registry from all `graphics/` files across mods using Graphics helpers.
// Returns true if registry was rebuilt (e.g., on first call or change).
bool cheap_scan_mods_to_update_sprite_name_registry();

// Build rich sprite store by scanning manifests and images, updating Graphics sprites.
// Returns true if store was rebuilt.
bool scan_mods_for_sprite_defs();

// Destroy global ModManager and release memory.
void cleanup_mods_manager();
