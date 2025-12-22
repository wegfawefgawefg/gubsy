#include "main_menu/menu_internal.hpp"

#include "demo_items.hpp"
#include "engine/audio.hpp"
#include "engine/graphics.hpp"
#include "globals.hpp"
#include "mods.hpp"
#include "state.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {

constexpr const char* kCatalogPath = "/mods/catalog";
constexpr const char* kFilesPrefix = "/mods/files/";

struct EndpointInfo {
    std::string host;
    int port{80};
};

std::string to_lower_copy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool parse_endpoint(const std::string& url, EndpointInfo& out, std::string& err) {
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

std::string url_encode_path(const std::string& path) {
    std::ostringstream oss;
    for (char raw : path) {
        unsigned char c = static_cast<unsigned char>(raw);
        bool safe = std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/' || c == '~';
        if (safe)
            oss << c;
        else
            oss << '%' << std::uppercase << std::hex << (int)c << std::nouppercase << std::dec;
    }
    return oss.str();
}

fs::path local_mod_path(const ModCatalogEntry& entry) {
    std::string folder = entry.folder.empty() ? entry.id : entry.folder;
    return fs::path("mods") / folder;
}

void refresh_runtime_after_mod_change() {
    discover_mods();
    scan_mods_for_sprite_defs();
    load_all_textures_in_sprite_lookup();
    load_mod_sounds();
    if (demo_items_active())
        load_demo_item_defs();
}

void update_install_flags() {
    if (!ss)
        return;
    for (auto& entry : ss->menu.mods_catalog) {
        std::error_code ec;
        bool installed = fs::exists(local_mod_path(entry), ec) && !entry.required;
        if (entry.required)
            installed = true;
        entry.installed = installed;
        entry.installing = false;
        entry.uninstalling = false;
        if (entry.status_text.empty())
            entry.status_text = installed ? std::string("Installed") : std::string("Not installed");
    }
}

bool fetch_catalog(std::vector<ModCatalogEntry>& out, std::string& err) {
    if (!ss)
        return false;
    EndpointInfo endpoint;
    if (!parse_endpoint(ss->menu.mods_server_url, endpoint, err))
        return false;
    httplib::Client cli(endpoint.host, endpoint.port);
    cli.set_read_timeout(5, 0);
    auto res = cli.Get(kCatalogPath);
    if (!res) {
        err = "Failed to reach mod server";
        return false;
    }
    if (res->status != 200) {
        err = "Catalog request failed with status " + std::to_string(res->status);
        return false;
    }
    try {
        nlohmann::json j = nlohmann::json::parse(res->body);
        auto mods_it = j.find("mods");
        if (mods_it == j.end() || !mods_it->is_array()) {
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

struct InstallCtx {
    httplib::Client* client{nullptr};
    std::unordered_set<std::string> installing;
};

bool download_file(InstallCtx& ctx, const ModCatalogEntry& entry, const ModFileEntry& file,
                   std::string& err) {
    if (!ctx.client) {
        err = "HTTP client not initialized";
        return false;
    }
    std::string url = kFilesPrefix + entry.id + "/" + url_encode_path(file.path);
    auto res = ctx.client->Get(url.c_str());
    if (!res || res->status != 200) {
        err = "Failed to download " + file.path;
        return false;
    }
    fs::path dest = local_mod_path(entry) / file.path;
    std::error_code ec;
    fs::create_directories(dest.parent_path(), ec);
    std::ofstream out(dest, std::ios::binary);
    if (!out.good()) {
        err = "Failed to write " + dest.string();
        return false;
    }
    out << res->body;
    return true;
}

bool install_recursive(ModCatalogEntry& entry, InstallCtx& ctx, std::string& err) {
    if (entry.required) {
        entry.installed = true;
        return true;
    }
    if (entry.installed)
        return true;
    if (ctx.installing.count(entry.id)) {
        err = "Dependency cycle detected involving " + entry.id;
        return false;
    }
    ctx.installing.insert(entry.id);

    for (const auto& dep : entry.dependencies) {
        ModCatalogEntry* dep_entry = find_mod_entry(dep);
        if (!dep_entry) {
            err = "Missing dependency: " + dep;
            ctx.installing.erase(entry.id);
            return false;
        }
        if (!install_recursive(*dep_entry, ctx, err)) {
            ctx.installing.erase(entry.id);
            return false;
        }
    }

    ctx.installing.erase(entry.id);
    entry.installing = true;
    entry.status_text = "Installing...";
    std::error_code ec;
    fs::remove_all(local_mod_path(entry), ec);
    fs::create_directories(local_mod_path(entry), ec);

    int completed = 0;
    for (const auto& file : entry.files) {
        if (!download_file(ctx, entry, file, err)) {
            entry.installing = false;
            entry.status_text = "Install failed";
            fs::remove_all(local_mod_path(entry), ec);
            return false;
        }
        completed += 1;
        entry.status_text = "Installing " + std::to_string(completed) + "/" +
                            std::to_string(entry.files.size());
    }
    entry.installing = false;
    entry.installed = true;
    entry.status_text = "Installed";
    ss->menu.mods_install_state[entry.id] = true;
    return true;
}

bool uninstall_entry(ModCatalogEntry& entry, std::string& err) {
    if (entry.required) {
        err = "Core mods cannot be removed";
        return false;
    }
    for (const auto& candidate : ss->menu.mods_catalog) {
        if (!candidate.installed || candidate.id == entry.id)
            continue;
        if (std::find(candidate.dependencies.begin(), candidate.dependencies.end(), entry.id) !=
            candidate.dependencies.end()) {
            err = "Required by " + candidate.title;
            return false;
        }
    }
    entry.uninstalling = true;
    entry.status_text = "Removing...";
    std::error_code ec;
    fs::remove_all(local_mod_path(entry), ec);
    entry.uninstalling = false;
    entry.installed = false;
    entry.status_text = "Not installed";
    ss->menu.mods_install_state[entry.id] = false;
    return true;
}

} // namespace

std::string trim_copy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

ModCatalogEntry* find_mod_entry(const std::string& id) {
    if (!ss)
        return nullptr;
    for (auto& entry : ss->menu.mods_catalog) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

const std::vector<ModCatalogEntry>& menu_mod_catalog() {
    static const std::vector<ModCatalogEntry> empty;
    if (!ss)
        return empty;
    return ss->menu.mods_catalog;
}

bool ensure_mod_catalog_loaded() {
    if (!ss)
        return false;
    if (ss->menu.mods_catalog_loaded)
        return true;
    if (ss->menu.mods_catalog_loading)
        return false;
    ss->menu.mods_catalog_loading = true;
    std::vector<ModCatalogEntry> entries;
    std::string err;
    bool ok = fetch_catalog(entries, err);
    if (ok) {
        ss->menu.mods_catalog = std::move(entries);
        ss->menu.mods_catalog_loaded = true;
        ss->menu.mods_catalog_error.clear();
        update_install_flags();
    } else {
        ss->menu.mods_catalog_error = err.empty() ? "Failed to fetch catalog" : err;
        ss->menu.mods_catalog_loaded = false;
    }
    ss->menu.mods_catalog_loading = false;
    if (ss->menu.mods_catalog_loaded)
        rebuild_mods_filter();
    return ss->menu.mods_catalog_loaded;
}

bool is_mod_installed(const std::string& id) {
    if (ModCatalogEntry* entry = find_mod_entry(id))
        return entry->installed;
    std::error_code ec;
    bool exists = fs::exists(fs::path("mods") / id, ec);
    return exists;
}

bool install_mod_by_index(int catalog_idx, std::string& error) {
    if (!ss)
        return false;
    if (catalog_idx < 0 || catalog_idx >= static_cast<int>(ss->menu.mods_catalog.size())) {
        error = "Invalid mod index";
        return false;
    }
    EndpointInfo endpoint;
    if (!parse_endpoint(ss->menu.mods_server_url, endpoint, error))
        return false;
    httplib::Client cli(endpoint.host, endpoint.port);
    cli.set_read_timeout(5, 0);
    InstallCtx ctx;
    ctx.client = &cli;
    ModCatalogEntry& entry = ss->menu.mods_catalog[static_cast<std::size_t>(catalog_idx)];
    bool ok = install_recursive(entry, ctx, error);
    if (ok) {
        refresh_runtime_after_mod_change();
        update_install_flags();
    }
    return ok;
}

bool uninstall_mod_by_index(int catalog_idx, std::string& error) {
    if (!ss)
        return false;
    if (catalog_idx < 0 || catalog_idx >= static_cast<int>(ss->menu.mods_catalog.size())) {
        error = "Invalid mod index";
        return false;
    }
    ModCatalogEntry& entry = ss->menu.mods_catalog[static_cast<std::size_t>(catalog_idx)];
    bool ok = uninstall_entry(entry, error);
    if (ok) {
        refresh_runtime_after_mod_change();
        update_install_flags();
    }
    return ok;
}

void rebuild_mods_filter() {
    if (!ss)
        return;
    ss->menu.mods_visible_indices.clear();
    std::string query = to_lower_copy(ss->menu.mods_search_query);
    for (std::size_t i = 0; i < ss->menu.mods_catalog.size(); ++i) {
        const auto& entry = ss->menu.mods_catalog[i];
        if (!query.empty()) {
            std::string haystack = to_lower_copy(entry.title + " " + entry.summary + " " + entry.author);
            if (haystack.find(query) == std::string::npos)
                continue;
        }
        ss->menu.mods_visible_indices.push_back(static_cast<int>(i));
    }
    ss->menu.mods_filtered_count = static_cast<int>(ss->menu.mods_visible_indices.size());
    if (ss->menu.mods_filtered_count == 0) {
        ss->menu.mods_total_pages = 1;
        ss->menu.mods_catalog_page = 0;
    } else {
        ss->menu.mods_total_pages = std::max(1, (ss->menu.mods_filtered_count + kModsPerPage - 1) / kModsPerPage);
        ss->menu.mods_catalog_page = std::clamp(ss->menu.mods_catalog_page, 0, ss->menu.mods_total_pages - 1);
    }
}

void enter_mods_page() {
    if (!ss)
        return;
    ensure_mod_catalog_loaded();
    rebuild_mods_filter();
}
