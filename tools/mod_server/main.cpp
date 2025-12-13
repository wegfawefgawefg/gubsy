#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct RepoFile {
    std::string path;
    std::uint64_t size_bytes{0};
};

struct RepoMod {
    std::string id;
    std::string title;
    std::string author;
    std::string version;
    std::string description;
    std::vector<std::string> dependencies;
    bool required{false};
    fs::path root;
    std::vector<RepoFile> files;
    std::uint64_t total_bytes{0};
};

struct RepoState {
    std::vector<RepoMod> mods;
    fs::path root;

    const RepoMod* find(const std::string& id) const {
        for (const auto& mod : mods) {
            if (mod.id == id)
                return &mod;
        }
        return nullptr;
    }
};

bool load_manifest(const fs::path& manifest_path, RepoMod& mod) {
    std::ifstream f(manifest_path);
    if (!f.good())
        return false;
    try {
        nlohmann::json j;
        f >> j;
        if (!j.is_object())
            return false;
        auto fetch_string = [&](const char* key, std::string& dst) {
            auto it = j.find(key);
            if (it != j.end() && it->is_string())
                dst = it->get<std::string>();
        };
        fetch_string("id", mod.id);
        fetch_string("title", mod.title);
        fetch_string("author", mod.author);
        fetch_string("version", mod.version);
        fetch_string("description", mod.description);
        if (auto it = j.find("dependencies"); it != j.end() && it->is_array()) {
            mod.dependencies.clear();
            for (auto& dep : *it) {
                if (dep.is_string())
                    mod.dependencies.push_back(dep.get<std::string>());
            }
        }
        if (auto it = j.find("required"); it != j.end() && it->is_boolean())
            mod.required = it->get<bool>();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[mod_server] Failed to parse " << manifest_path << ": " << e.what() << "\n";
        return false;
    }
}

RepoState build_repo(const fs::path& root) {
    RepoState state;
    state.root = root;
    fs::path mods_dir = root / "mods";
    std::error_code ec;
    if (!fs::exists(mods_dir, ec) || !fs::is_directory(mods_dir, ec)) {
        std::cerr << "[mod_server] Mods directory missing: " << mods_dir << "\n";
        return state;
    }
    for (auto const& entry : fs::directory_iterator(mods_dir, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_directory())
            continue;
        RepoMod mod{};
        mod.root = entry.path();
        fs::path manifest = mod.root / "manifest.json";
        if (!load_manifest(manifest, mod)) {
            std::cerr << "[mod_server] Skipping " << entry.path() << " (missing manifest)\n";
            continue;
        }
        if (mod.id.empty())
            mod.id = entry.path().filename().string();
        mod.files.clear();
        mod.total_bytes = 0;
        for (auto const& f : fs::recursive_directory_iterator(entry.path(), ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!f.is_regular_file())
                continue;
            fs::path rel = fs::relative(f.path(), mod.root, ec);
            if (ec) {
                ec.clear();
                continue;
            }
            RepoFile rf;
            rf.path = rel.generic_string();
            rf.size_bytes = fs::file_size(f.path(), ec);
            if (ec) {
                ec.clear();
                continue;
            }
            mod.total_bytes += rf.size_bytes;
            mod.files.push_back(std::move(rf));
        }
        state.mods.push_back(std::move(mod));
    }
    return state;
}

nlohmann::json build_catalog_json(const RepoState& state) {
    nlohmann::json catalog;
    catalog["mods"] = nlohmann::json::array();
    for (auto const& mod : state.mods) {
        nlohmann::json entry;
        entry["id"] = mod.id;
        entry["title"] = mod.title;
        entry["author"] = mod.author;
        entry["version"] = mod.version;
        entry["description"] = mod.description;
        entry["dependencies"] = mod.dependencies;
        entry["required"] = mod.required;
        entry["folder"] = mod.root.filename().string();
        entry["size_bytes"] = mod.total_bytes;
        nlohmann::json files = nlohmann::json::array();
        for (auto const& file : mod.files) {
            files.push_back({
                {"path", file.path},
                {"size_bytes", file.size_bytes},
            });
        }
        entry["files"] = std::move(files);
        catalog["mods"].push_back(std::move(entry));
    }
    return catalog;
}

bool is_subpath(const fs::path& base, const fs::path& target) {
    auto norm_base = fs::weakly_canonical(base);
    auto norm_target = fs::weakly_canonical(target);
    auto it_base = norm_base.begin();
    auto it_target = norm_target.begin();
    for (; it_base != norm_base.end() && it_target != norm_target.end(); ++it_base, ++it_target) {
        if (*it_base != *it_target)
            return false;
    }
    return it_base == norm_base.end();
}

int main(int argc, char** argv) {
    fs::path root = "mod_repo";
    int port = 8787;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            root = argv[++i];
        } else if (arg.rfind("--port=", 0) == 0) {
            try {
                port = std::stoi(arg.substr(7));
            } catch (...) {
                port = 8787;
            }
        } else if (arg == "--help") {
            std::cout << "Usage: mod_server [--root <path>] [--port=<port>]\n";
            return 0;
        }
    }

    RepoState state = build_repo(root);
    if (state.mods.empty()) {
        std::cerr << "[mod_server] No mods found under " << root << "/mods\n";
    } else {
        std::cout << "[mod_server] Loaded " << state.mods.size() << " mods from " << root << "\n";
    }
    nlohmann::json catalog_json = build_catalog_json(state);

    httplib::Server server;
    server.Get("/mods/catalog", [catalog_json](const httplib::Request&, httplib::Response& res) {
        res.set_content(catalog_json.dump(2), "application/json");
    });
    server.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });
    server.Get(R"(/mods/files/([^/]+)/(.+))",
               [&state](const httplib::Request& req, httplib::Response& res) {
                   if (req.matches.size() < 3) {
                       res.status = 400;
                       res.set_content("bad request", "text/plain");
                       return;
                   }
                   std::string mod_id = req.matches[1];
                   std::string rel_path = req.matches[2];
                   const RepoMod* mod = state.find(mod_id);
                   if (!mod) {
                       res.status = 404;
                       res.set_content("mod not found", "text/plain");
                       return;
                   }
                   fs::path target = mod->root / fs::path(rel_path);
                   if (!is_subpath(mod->root, target) || !fs::exists(target) || !fs::is_regular_file(target)) {
                       res.status = 404;
                       res.set_content("file not found", "text/plain");
                       return;
                   }
                   std::ifstream f(target, std::ios::binary);
                   if (!f.good()) {
                       res.status = 500;
                       res.set_content("failed to read file", "text/plain");
                       return;
                   }
                   std::string buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                   res.set_content(std::move(buf), "application/octet-stream");
               });

    std::cout << "[mod_server] Listening on http://127.0.0.1:" << port << "\n";
    server.listen("0.0.0.0", port);
    return 0;
}
