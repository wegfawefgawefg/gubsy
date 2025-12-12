#include "mods.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <optional>
#include <system_error>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

static bool starts_with(const std::string& s, const char* pfx) {
    return s.rfind(pfx, 0) == 0;
}

void ModManager::track_tree(const std::string& path) {
    std::error_code ec;
    fs::path p(path);
    if (!fs::exists(p, ec))
        return;
    if (fs::is_regular_file(p, ec)) {
        tracked_files[p.string()] = fs::last_write_time(p, ec);
        return;
    }
    if (!fs::is_directory(p, ec))
        return;
    for (auto const& entry : fs::recursive_directory_iterator(p, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (entry.is_regular_file()) {
            auto sp = entry.path().string();
            tracked_files[sp] = fs::last_write_time(entry.path(), ec);
        }
    }
}

bool ModManager::check_changes(std::vector<std::string>& changed_assets,
                                std::vector<std::string>& changed_scripts) {
    bool any = false;
    std::error_code ec;
    // Re-scan tracked files and detect new/removed/modified
    std::unordered_map<std::string, fs::file_time_type> current;
    current.reserve(tracked_files.size());
    for (auto const& m : mods) {
        // manifest or info
        if (!m.manifest_path.empty()) {
            fs::path mp = m.manifest_path;
            if (fs::exists(mp, ec))
                current[mp.string()] = fs::last_write_time(mp, ec);
        } else {
            fs::path ip = fs::path(m.path) / "info.toml";
            if (fs::exists(ip, ec))
                current[ip.string()] = fs::last_write_time(ip, ec);
        }
        // graphics
        fs::path gdir = fs::path(m.path) / "graphics";
        if (fs::exists(gdir, ec) && fs::is_directory(gdir, ec)) {
            for (auto const& e : fs::recursive_directory_iterator(gdir, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (e.is_regular_file())
                    current[e.path().string()] = fs::last_write_time(e.path(), ec);
            }
        }
        // scripts
        fs::path sdir = fs::path(m.path) / "scripts";
        if (fs::exists(sdir, ec) && fs::is_directory(sdir, ec)) {
            for (auto const& e : fs::recursive_directory_iterator(sdir, ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (e.is_regular_file())
                    current[e.path().string()] = fs::last_write_time(e.path(), ec);
            }
        }
    }

    // Compare with previous snapshot
    for (auto const& [path, ts] : current) {
        auto it = tracked_files.find(path);
        if (it == tracked_files.end()) {
            any = true;
            if (path.find("/graphics/") != std::string::npos)
                changed_assets.push_back(path);
            else if (path.find("/scripts/") != std::string::npos)
                changed_scripts.push_back(path);
            else if (path.rfind("info.toml") != std::string::npos ||
                     path.rfind("manifest.json") != std::string::npos) {
                changed_scripts.push_back(path);
            }
        } else if (ts != it->second) {
            any = true;
            if (path.find("/graphics/") != std::string::npos)
                changed_assets.push_back(path);
            else if (path.find("/scripts/") != std::string::npos)
                changed_scripts.push_back(path);
            else if (path.rfind("info.toml") != std::string::npos ||
                     path.rfind("manifest.json") != std::string::npos) {
                changed_scripts.push_back(path);
            }
        }
    }
    // Detect deletions
    for (auto const& [path, _] : tracked_files) {
        if (current.find(path) == current.end()) {
            any = true;
            if (path.find("/graphics/") != std::string::npos)
                changed_assets.push_back(path);
            else if (path.find("/scripts/") != std::string::npos ||
                     path.rfind("manifest.json") != std::string::npos ||
                     path.rfind("info.toml") != std::string::npos)
                changed_scripts.push_back(path);
        }
    }
    // Update snapshot
    tracked_files = std::move(current);
    return any;
}

namespace {

bool parse_manifest_json(const std::string& manifest_path, ModInfo& info) {
    std::ifstream f(manifest_path);
    if (!f.good())
        return false;
    try {
        nlohmann::json j;
        f >> j;
        if (!j.is_object())
            return false;
        info.manifest_path = manifest_path;
        if (auto it = j.find("id"); it != j.end() && it->is_string())
            info.name = it->get<std::string>();
        if (auto it = j.find("slug"); it != j.end() && it->is_string() && info.name.empty())
            info.name = it->get<std::string>();
        if (auto it = j.find("title"); it != j.end() && it->is_string())
            info.title = it->get<std::string>();
        if (auto it = j.find("name"); it != j.end() && it->is_string()) {
            if (info.name.empty())
                info.name = it->get<std::string>();
            if (info.title.empty())
                info.title = it->get<std::string>();
        }
        if (auto it = j.find("version"); it != j.end() && it->is_string())
            info.version = it->get<std::string>();
        if (auto it = j.find("description"); it != j.end() && it->is_string())
            info.description = it->get<std::string>();
        if (auto it = j.find("author"); it != j.end() && it->is_string())
            info.author = it->get<std::string>();
        if (auto it = j.find("download_url"); it != j.end() && it->is_string())
            info.download_url = it->get<std::string>();
        if (auto it = j.find("dependencies"); it != j.end() && it->is_array()) {
            for (auto& dep : *it) {
                if (dep.is_string())
                    info.deps.push_back(dep.get<std::string>());
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::printf("[mods] Failed to parse manifest %s: %s\n",
                    manifest_path.c_str(), e.what());
        return false;
    }
}

void parse_info_toml(const std::string& info_path, ModInfo& info) {
    std::ifstream f(info_path);
    if (!f.good())
        return;
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#')
            continue;
        if (starts_with(t, "name")) {
            auto pos = t.find('=');
            if (pos != std::string::npos) {
                std::string v = trim(t.substr(pos + 1));
                if (!v.empty() && v.front() == '"' && v.back() == '"')
                    v = v.substr(1, v.size() - 2);
                info.name = v;
            }
        } else if (starts_with(t, "version")) {
            auto pos = t.find('=');
            if (pos != std::string::npos) {
                std::string v = trim(t.substr(pos + 1));
                if (!v.empty() && v.front() == '"' && v.back() == '"')
                    v = v.substr(1, v.size() - 2);
                info.version = v;
            }
        } else if (starts_with(t, "deps")) {
            auto pos = t.find('=');
            if (pos != std::string::npos) {
                std::string v = trim(t.substr(pos + 1));
                if (!v.empty() && v.front() == '[' && v.back() == ']') {
                    std::string inner = v.substr(1, v.size() - 2);
                    size_t i = 0;
                    while (i < inner.size()) {
                        while (i < inner.size() &&
                               std::isspace(static_cast<unsigned char>(inner[i])))
                            ++i;
                        if (i < inner.size() && inner[i] == '"') {
                            ++i;
                            size_t j = i;
                            while (j < inner.size() && inner[j] != '"')
                                ++j;
                            if (j < inner.size()) {
                                info.deps.push_back(inner.substr(i, j - i));
                                i = j + 1;
                            }
                        }
                        while (i < inner.size() && inner[i] != ',')
                            ++i;
                        if (i < inner.size() && inner[i] == ',')
                            ++i;
                    }
                }
            }
        }
    }
}

} // namespace

ModInfo ModManager::parse_info(const std::string& mod_path) {
    ModInfo info{};
    info.path = mod_path;
    const std::string manifest_path = mod_path + "/manifest.json";
    if (!parse_manifest_json(manifest_path, info)) {
        parse_info_toml(mod_path + "/info.toml", info);
    }
    if (info.name.empty())
        info.name = fs::path(mod_path).filename().string();
    if (info.title.empty())
        info.title = info.name;
    if (info.version.empty())
        info.version = "0.0.0";
    return info;
}
