#include "engine/mod_install.hpp"

#include "engine/mod_host.hpp"
#include "engine/mods.hpp"
#include "engine/graphics.hpp"
#include "engine/audio.hpp"
#include "engine/globals.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include <httplib/httplib.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <nlohmann/json.hpp>

namespace {

constexpr const char* kCatalogPath = "/mods/catalog";
constexpr const char* kFilesPrefix = "/mods/files/";

struct EndpointInfo {
    std::string host;
    int port{80};
};

std::string url_encode_path(const std::string& path) {
    std::ostringstream oss;
    for (char raw : path) {
        unsigned char c = static_cast<unsigned char>(raw);
        bool safe = std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/' || c == '~';
        if (safe)
            oss << c;
        else
            oss << '%' << std::uppercase << std::hex << static_cast<int>(c)
                << std::nouppercase << std::dec;
    }
    return oss.str();
}

bool parse_http_endpoint(const std::string& url, EndpointInfo& out, std::string& err) {
    std::string work = url;
    std::string prefix = "http://";
    if (work.rfind(prefix, 0) != 0) {
        err = "Only http:// URLs are supported";
        return false;
    }
    work = work.substr(prefix.size());
    auto slash = work.find('/');
    if (slash != std::string::npos)
        work = work.substr(0, slash);
    if (work.empty()) {
        err = "Missing host in server URL";
        return false;
    }
    std::string host = work;
    int port = 80;
    auto colon = work.find(':');
    if (colon != std::string::npos) {
        host = work.substr(0, colon);
        std::string port_str = work.substr(colon + 1);
        try {
            port = std::stoi(port_str);
        } catch (...) {
            err = "Invalid port in server URL";
            return false;
        }
    }
    if (host.empty()) {
        err = "Invalid host in server URL";
        return false;
    }
    out.host = host;
    out.port = port;
    return true;
}

namespace fs = std::filesystem;

fs::path mods_root_path() {
    if (mm && !mm->root.empty())
        return fs::path(mm->root);
    return fs::path("mods");
}

fs::path local_mod_path(const ModCatalogEntry& entry) {
    std::string folder = entry.folder.empty() ? entry.id : entry.folder;
    if (folder.empty())
        folder = entry.id;
    return mods_root_path() / folder;
}

bool ensure_parent_dirs(const fs::path& path, std::string& err) {
    std::error_code ec;
    fs::path parent = path.parent_path();
    if (parent.empty())
        return true;
    if (fs::exists(parent, ec))
        return true;
    if (!fs::create_directories(parent, ec)) {
        err = "Failed to create directory: " + parent.string();
        return false;
    }
    return true;
}

void refresh_runtime(const std::vector<std::string>& previously_active) {
    discover_mods();
    scan_mods_for_sprite_defs();
    load_all_textures_in_sprite_lookup();
    load_mod_sounds();
    set_active_mods(previously_active);
}

} // namespace

bool fetch_mod_catalog(const std::string& server_url,
                       std::vector<ModCatalogEntry>& out,
                       std::string& err) {
    EndpointInfo endpoint;
    if (!parse_http_endpoint(server_url, endpoint, err))
        return false;
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(5, 0);
    auto res = client.Get(kCatalogPath);
    if (!res) {
        err = "Failed to reach mod server";
        return false;
    }
    if (res->status != 200) {
        err = "Catalog request failed with status " + std::to_string(res->status);
        return false;
    }
    try {
        nlohmann::json root = nlohmann::json::parse(res->body);
        auto mods_it = root.find("mods");
        if (mods_it == root.end() || !mods_it->is_array()) {
            err = "Malformed catalog response";
            return false;
        }
        out.clear();
        for (auto& entry : *mods_it) {
            if (!entry.is_object())
                continue;
            ModCatalogEntry cat;
            cat.id = entry.value("id", "");
            cat.folder = entry.value("folder", cat.id);
            cat.title = entry.value("title", cat.id);
            cat.author = entry.value("author", "Unknown");
            cat.version = entry.value("version", "0.0.0");
            cat.summary = entry.value("description", "");
            cat.description = cat.summary;
            cat.required = entry.value("required", false);
            cat.game_version = entry.value("game_version", "");
            cat.total_bytes = entry.value("size_bytes", static_cast<std::uint64_t>(0));
            if (entry.contains("dependencies") && entry["dependencies"].is_array()) {
                for (auto& dep : entry["dependencies"]) {
                    if (dep.is_string())
                        cat.dependencies.push_back(dep.get<std::string>());
                }
            }
            if (entry.contains("apis") && entry["apis"].is_array()) {
                for (auto& api : entry["apis"]) {
                    if (api.is_string())
                        cat.apis.push_back(api.get<std::string>());
                }
            }
            if (entry.contains("files") && entry["files"].is_array()) {
                for (auto& f : entry["files"]) {
                    if (!f.is_object())
                        continue;
                    ModFileEntry fe;
                    fe.path = f.value("path", "");
                    fe.size_bytes = f.value("size_bytes", static_cast<std::uint64_t>(0));
                    if (!fe.path.empty())
                        cat.files.push_back(std::move(fe));
                }
            }
            if (!cat.id.empty())
                out.push_back(std::move(cat));
        }
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

bool install_mod_from_catalog(const std::string& server_url,
                              const ModCatalogEntry& entry,
                              std::string& err) {
    EndpointInfo endpoint;
    if (!parse_http_endpoint(server_url, endpoint, err))
        return false;
    if (entry.id.empty()) {
        err = "Catalog entry missing id";
        return false;
    }
    httplib::Client client(endpoint.host, endpoint.port);
    client.set_read_timeout(5, 0);

    fs::path target = local_mod_path(entry);
    std::error_code ec;
    if (fs::exists(target, ec))
        fs::remove_all(target, ec);
    ec.clear();
    if (!fs::create_directories(target, ec) && ec) {
        err = "Failed to create mod directory: " + target.string();
        return false;
    }

    auto active = get_active_mod_ids();

    for (const auto& file : entry.files) {
        if (file.path.empty())
            continue;
        fs::path dest = target / file.path;
        if (!ensure_parent_dirs(dest, err))
            return false;
        std::string url = std::string(kFilesPrefix) + entry.id + "/" + url_encode_path(file.path);
        auto res = client.Get(url.c_str());
        if (!res) {
            err = "Failed to download " + file.path;
            return false;
        }
        if (res->status != 200) {
            err = "Download failed (" + std::to_string(res->status) + ") for " + file.path;
            return false;
        }
        std::ofstream out(dest, std::ios::binary);
        if (!out.good()) {
            err = "Failed to write " + dest.string();
            return false;
        }
        out << res->body;
    }

    refresh_runtime(active);
    return true;
}

bool uninstall_mod(const ModCatalogEntry& entry, std::string& err) {
    if (entry.id.empty()) {
        err = "Missing mod id";
        return false;
    }
    fs::path root = local_mod_path(entry);
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        err = "Mod not installed: " + root.string();
        return false;
    }

    auto active = get_active_mod_ids();
    active.erase(std::remove(active.begin(), active.end(), entry.id), active.end());
    deactivate_mod(entry.id);

    fs::remove_all(root, ec);
    if (ec) {
        err = "Failed to remove " + root.string();
        return false;
    }

    refresh_runtime(active);
    return true;
}
