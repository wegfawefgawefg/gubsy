#include "mods.hpp"
#include "globals.hpp"
#include "settings.hpp"
#include "engine/graphics.hpp"
#include "demo_items.hpp"
#include "demo_content.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <optional>
#include <queue>
#include <system_error>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

constexpr const char* kModsEnabledConfig = "config/mods_enabled.json";

struct ModsConfig {
    bool has_enabled_list{false};
    std::unordered_set<std::string> enabled;
    std::unordered_set<std::string> disabled;
};

std::optional<ModsConfig> read_mods_config() {
    fs::path path = kModsEnabledConfig;
    std::ifstream f(path);
    if (!f.good())
        return std::nullopt;
    try {
        nlohmann::json j;
        f >> j;
        if (!j.is_object())
            return ModsConfig{};
        ModsConfig cfg;
        if (auto it = j.find("enabled"); it != j.end() && it->is_array()) {
            cfg.has_enabled_list = true;
            for (auto& val : *it) {
                if (val.is_string())
                    cfg.enabled.insert(val.get<std::string>());
            }
        }
        if (auto it = j.find("disabled"); it != j.end() && it->is_array()) {
            for (auto& val : *it) {
                if (val.is_string())
                    cfg.disabled.insert(val.get<std::string>());
            }
        }
        return cfg;
    } catch (const std::exception& e) {
        std::printf("[mods] Failed to parse %s: %s\n", path.string().c_str(), e.what());
        return std::nullopt;
    }
}

void ensure_mods_config_exists() {
    fs::path path = kModsEnabledConfig;
    std::error_code ec;
    if (fs::exists(path, ec))
        return;
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path(), ec);
    nlohmann::json j;
    j["disabled"] = nlohmann::json::array();
    std::ofstream out(path);
    if (!out.good()) {
        std::printf("[mods] Failed to write %s\n", path.string().c_str());
        return;
    }
    out << j.dump(2) << "\n";
}

std::vector<ModInfo> resolve_mod_order(std::vector<ModInfo> mods) {
    std::unordered_map<std::string, size_t> id_to_index;
    std::vector<bool> valid(mods.size(), true);
    for (size_t i = 0; i < mods.size(); ++i) {
        auto& m = mods[i];
        if (m.name.empty())
            m.name = fs::path(m.path).filename().string();
        if (m.title.empty())
            m.title = m.name;
        if (m.version.empty())
            m.version = "0.0.0";
        auto [it, inserted] = id_to_index.emplace(m.name, i);
        if (!inserted) {
            std::printf("[mods] Duplicate mod id '%s' (skipping %s)\n",
                        m.name.c_str(), m.path.c_str());
            valid[i] = false;
            mods[i].enabled = false;
        }
    }

    std::vector<std::vector<size_t>> graph(mods.size());
    std::vector<int> indegree(mods.size(), 0);
    std::vector<bool> active(mods.size(), false);

    for (size_t i = 0; i < mods.size(); ++i) {
        if (!valid[i] || !mods[i].enabled)
            continue;
        bool deps_ok = true;
        for (const auto& dep : mods[i].deps) {
            auto dit = id_to_index.find(dep);
            if (dit == id_to_index.end()) {
                std::printf("[mods] %s depends on missing mod '%s'; skipping\n",
                            mods[i].name.c_str(), dep.c_str());
                deps_ok = false;
                break;
            }
            if (!mods[dit->second].enabled) {
                std::printf("[mods] %s depends on disabled mod '%s'; skipping\n",
                            mods[i].name.c_str(), dep.c_str());
                deps_ok = false;
                break;
            }
        }
        if (!deps_ok) {
            mods[i].enabled = false;
            continue;
        }
        active[i] = true;
        for (const auto& dep : mods[i].deps) {
            size_t dep_idx = id_to_index[dep];
            graph[dep_idx].push_back(i);
            indegree[i] += 1;
        }
    }

    std::queue<size_t> q;
    std::vector<bool> visited(mods.size(), false);
    for (size_t i = 0; i < mods.size(); ++i) {
        if (active[i] && indegree[i] == 0)
            q.push(i);
    }

    std::vector<ModInfo> ordered;
    ordered.reserve(mods.size());
    while (!q.empty()) {
        size_t idx = q.front();
        q.pop();
        visited[idx] = true;
        ordered.push_back(std::move(mods[idx]));
        for (size_t next : graph[idx]) {
            if (--indegree[next] == 0)
                q.push(next);
        }
    }

    for (size_t i = 0; i < mods.size(); ++i) {
        if (active[i] && !visited[i]) {
            std::printf("[mods] Dependency cycle involving '%s'; skipping\n",
                        mods[i].name.c_str());
        }
    }

    return ordered;
}

} // namespace

bool init_mods_manager(const std::string& mods_root) {
    ModManager* m = new ModManager{};
    m->root = mods_root;
    mm = m;
    return true;
}

void cleanup_mods_manager() {
    if (mm) {
        delete mm;
        mm = nullptr;
    }
}

/// Discover available mods by scanning `mods/*/` for `info.toml`.
/// Clears any previously discovered mods.
void discover_mods() {
    mm->mods.clear();
    std::vector<ModInfo> discovered;
    std::error_code ec;
    if (!fs::exists(mm->root, ec) || !fs::is_directory(mm->root, ec))
        return;
    for (auto const& e : fs::directory_iterator(mm->root, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!e.is_directory())
            continue;
        auto p = e.path();
        ModInfo mi = mm->parse_info(p.string());
        discovered.push_back(std::move(mi));
    }

    ensure_mods_config_exists();
    auto cfg = read_mods_config();
    if (cfg) {
        for (auto& mod : discovered) {
            bool enabled = true;
            if (cfg->has_enabled_list) {
                enabled = cfg->enabled.find(mod.name) != cfg->enabled.end();
            } else if (!cfg->disabled.empty()) {
                enabled = cfg->disabled.find(mod.name) == cfg->disabled.end();
            }
            mod.enabled = enabled;
        }
    } else {
        for (auto& mod : discovered)
            mod.enabled = true;
    }

    std::vector<ModInfo> ordered = resolve_mod_order(std::move(discovered));
    mm->mods = std::move(ordered);

    // Track initial trees
    mm->tracked_files.clear();
    for (auto const& m : mm->mods) {
        mm->track_tree(m.path + "/graphics");
        mm->track_tree(m.path + "/scripts");
        if (!m.manifest_path.empty())
            mm->track_tree(m.manifest_path);
        else
            mm->track_tree(m.path + "/info.toml");
    }
}


/// Re-scan mods and rebuild the sprite name↔id registry.
///
/// Walks each `<mod.path>/graphics` recursively, collects file stems, and
/// namespaces them as `<mod.name>:<stem>`. Skips non-regular files. Clears and
/// ignores transient `std::error_code`s during traversal.
///
/// Calls `rebuild_sprite_registry(names)`, which nukes and replaces:
///   - Graphics::sprite_name_to_id
///   - Graphics::sprite_id_to_name
///
/// Does NOT parse manifests, build SpriteDef data, or load textures.
/// Sets `registry_built = true`.
///
/// ⚠ ID stability: IDs are reassigned based on the collected order, so they may change.
///
/// Returns `true` on completion (does not signal whether anything changed).
/// Complexity: O(#files). Call when you need a fast index refresh after add/remove/rename;
/// use the full store rebuild for manifest/content changes.
bool cheap_scan_mods_to_update_sprite_name_registry() {
    std::vector<std::string> names;
    std::error_code ec;
    for (auto const& m : mm->mods) {
        fs::path gdir = fs::path(m.path) / "graphics";
        if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
            continue;
        for (auto const& entry : fs::recursive_directory_iterator(gdir, ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!entry.is_regular_file())
                continue;
            auto stem = entry.path().stem().string();
            if (!stem.empty()) {
                std::string ns = m.name + ":" + stem;
                names.push_back(ns);
            }
        }
    }
    build_sprite_name_id_mapping(names);
    mm->registry_built = true;
    std::printf("[mods] Sprite registry built with %zu entries\n", names.size());
    return true;
}

static bool is_manifest_ext(const std::string& ext) {
    return ext == ".sprite" || ext == ".sprite.toml";
}

static bool is_image_ext(const std::string& ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" ||
           ext == ".webp" || ext == ".tga";
}

/// Rebuild the full sprite store from all mods.
///
/// Scans each `<mod.path>/graphics` recursively. Prefers manifests
/// (`.sprite`, `.sprite.toml`) over bare images. For each entry:
///   - Parses manifest into `SpriteDef` (namespaced as `<mod.name>:<name>`).
///   - Resolves relative `image_path` against `<mod.path>/graphics/`.
///   - If no manifest exists, synthesizes a default `SpriteDef` from the image.
///
/// Uses first definition per name (later duplicates ignored). Sorts by name
/// for deterministic IDs, then calls `rebuild_sprite_mapping(defs)` which
/// replaces:
///   - `Graphics::sprite_defs_by_id`
///   - `Graphics::sprite_id_to_name`
///   - `Graphics::sprite_name_to_id`
///
/// Does NOT load textures. Log-and-continue on parse errors. Complexity
/// O(files + parse). Call on startup and whenever manifests or image content
/// change. IDs may change if the name set changes.
bool scan_mods_for_sprite_defs() {
    auto mod_infos = mm->mods;
    
    // First pass: collect manifests per name (prefer manifests over bare images)
    std::unordered_map<std::string, SpriteDef> defs_by_name;
    std::unordered_map<std::string, std::string>
        name_to_modroot; // for resolving relative image paths
    std::error_code ec;
    for (auto const& m : mod_infos) {
        fs::path gdir = fs::path(m.path) / "graphics";
        if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
            continue;
        for (auto const& entry : fs::recursive_directory_iterator(gdir, ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!entry.is_regular_file())
                continue;
            auto p = entry.path();
            std::string ext = p.extension().string();
            for (auto& c : ext)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (is_manifest_ext(ext)) {
                SpriteDef def{};
                std::string err;
                if (parse_sprite_manifest_file(p.string(), def, err)) {
                    // Namespace the sprite name if missing a prefix
                    if (def.name.find(':') == std::string::npos) {
                        def.name = m.name + ":" + def.name;
                    }
                    // Resolve image path relative to mod root if not absolute
                    if (!def.image_path.empty() && !fs::path(def.image_path).is_absolute()) {
                        def.image_path = (fs::path(m.path) / "graphics" / def.image_path)
                                             .lexically_normal()
                                             .string();
                    }
                    // Keep first definition for a name; later mods can override if desired (could
                    // be policy)
                    if (defs_by_name.find(def.name) == defs_by_name.end()) {
                        defs_by_name.emplace(def.name, std::move(def));
                        name_to_modroot[defs_by_name.find(def.name)->first] = m.path;
                    }
                } else {
                    std::printf("[mods] Sprite manifest parse failed: %s (%s)\n",
                                p.string().c_str(), err.c_str());
                }
            }
        }
    }
    // Second pass: images without manifests become default single-frame sprites
    for (auto const& m : mod_infos) {
        fs::path gdir = fs::path(m.path) / "graphics";
        if (!fs::exists(gdir, ec) || !fs::is_directory(gdir, ec))
            continue;
        for (auto const& entry : fs::recursive_directory_iterator(gdir, ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!entry.is_regular_file())
                continue;
            auto p = entry.path();
            std::string ext = p.extension().string();
            for (auto& c : ext)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!is_image_ext(ext))
                continue;
            std::string stem = p.stem().string();
            std::string nsname = m.name + ":" + stem;
            if (defs_by_name.find(nsname) != defs_by_name.end())
                continue; // manifest already defined
            SpriteDef d = make_default_sprite_from_image(nsname, p.string());
            defs_by_name.emplace(nsname, std::move(d));
            name_to_modroot[nsname] = m.path;
        }
    }

    // Deterministic ordering: sort by name before rebuilding
    std::vector<std::pair<std::string, SpriteDef>> sorted;
    sorted.reserve(defs_by_name.size());
    for (auto& kv : defs_by_name)
        sorted.emplace_back(kv.first, std::move(kv.second));
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<SpriteDef> defs;
    defs.reserve(sorted.size());
    for (auto& kv : sorted)
        defs.push_back(std::move(kv.second));
 
    rebuild_sprite_mapping(defs);
    std::printf("[mods] Sprite store built with %zu entries\n", defs.size());
    return true;
}


bool poll_fs_mods_hot_reload() {
    mm->accum_poll += ss->dt;
    if (mm->accum_poll < HOT_RELOAD_POLL_INTERVAL)
        return false;
    mm->accum_poll = 0.0f;

    std::vector<std::string> changed_assets;
    std::vector<std::string> changed_scripts;
    bool any = mm->check_changes(changed_assets, changed_scripts);
    if (!any)
        return false;

    if (!changed_assets.empty()) {
        std::printf("[mods] Asset changes detected (%zu). Rebuilding sprites...\n",
                    changed_assets.size());
        scan_mods_for_sprite_defs();
    }
    if (!changed_scripts.empty()) {
        std::printf("[mods] Script changes detected (%zu). Reloading demo scripts...\n",
                    changed_scripts.size());
        bool content_reloaded = load_demo_content();
        bool items_reloaded = load_demo_item_defs();
        if (content_reloaded || items_reloaded)
            ss->alerts.push_back({"Demo script reloaded", 0.0f, 1.5f, false});
    }
    return true;
}
