#include "engine/mod_host.hpp"

#include "engine/globals.hpp"
#include "engine/mod_api_registry.hpp"
#include "engine/mods.hpp"
#include "engine/graphics.hpp"
#include "engine/audio.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <unordered_set>
#include <sol/sol.hpp>

namespace {

std::unordered_map<std::string, ModContext> g_active_mods;
std::string g_required_version;

void rebuild_mod_assets() {
    scan_mods_for_sprite_defs();
    load_all_textures_in_sprite_lookup();
    load_mod_sounds();
}

ModInfo* find_mod_info_mutable(const std::string& id) {
    if (!mm)
        return nullptr;
    for (auto& info : mm->mods) {
        if (info.name == id)
            return &info;
    }
    return nullptr;
}

bool run_mod_scripts(ModContext& ctx) {
    namespace fs = std::filesystem;
    fs::path scripts_dir = fs::path(ctx.path) / "scripts";
    std::error_code ec;
    if (!fs::exists(scripts_dir, ec) || !fs::is_directory(scripts_dir, ec))
        return true;

    std::vector<fs::path> files;
    for (auto const& entry : fs::directory_iterator(scripts_dir, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_regular_file())
            continue;
        files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end());

    for (const auto& path : files) {
        auto ext = path.extension().string();
        for (auto& c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".lua")
            continue;
        sol::protected_function_result result =
            ctx.lua->safe_script_file(path.string(), sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            std::fprintf(stderr, "[mod_host] %s: %s\n",
                         path.string().c_str(), err.what());
        }
    }
    return true;
}

bool deactivate_internal(const std::string& id) {
    auto it = g_active_mods.find(id);
    if (it == g_active_mods.end())
        return false;
    for (const auto* api : it->second.bound_apis) {
        if (api->on_mod_unloaded)
            api->on_mod_unloaded(it->second);
    }
    g_active_mods.erase(it);
    return true;
}

bool activate_internal(ModInfo& info) {
    if (!info.enabled)
        return false;
    if (g_active_mods.count(info.name))
        return false;
    if (!g_required_version.empty() && !info.game_version.empty() &&
        info.game_version != g_required_version) {
        std::fprintf(stderr,
                     "[mod_host] Skipping %s: requires game_version %s (current %s)\n",
                     info.name.c_str(), info.game_version.c_str(),
                     g_required_version.c_str());
        return false;
    }

    ModContext ctx;
    ctx.id = info.name;
    ctx.title = info.title;
    ctx.path = info.path;
    ctx.requested_apis = info.apis;
    ctx.lua = std::make_unique<sol::state>();
    ctx.lua->open_libraries(sol::lib::base, sol::lib::math,
                            sol::lib::string, sol::lib::table);

    auto& registry = ModApiRegistry::instance();
    for (const auto& api_name : ctx.requested_apis) {
        if (const ModApiDescriptor* api = registry.find(api_name)) {
            if (api->bind_lua)
                api->bind_lua(*ctx.lua, ctx);
            ctx.bound_apis.push_back(api);
        } else {
            std::fprintf(stderr,
                         "[mod_host] Mod %s requested unknown API '%s'\n",
                         ctx.id.c_str(), api_name.c_str());
        }
    }

    run_mod_scripts(ctx);

    for (const auto* api : ctx.bound_apis) {
        if (api->on_mod_loaded)
            api->on_mod_loaded(ctx);
    }

    g_active_mods.emplace(ctx.id, std::move(ctx));
    return true;
}

} // namespace

void set_required_mod_game_version(const std::string& version) {
    g_required_version = version;
}

const std::string& required_mod_game_version() {
    return g_required_version;
}

bool load_enabled_mods_via_host() {
    if (!mm)
        return false;
    std::vector<std::string> enabled;
    for (const auto& mod : mm->mods) {
        if (mod.enabled)
            enabled.push_back(mod.name);
    }
    return set_active_mods(enabled);
}

bool reload_all_mods_via_host() {
    auto ids = get_active_mod_ids();
    if (ids.empty())
        return false;
    return reload_mods(ids);
}

void unload_all_mods_via_host() {
    std::vector<std::string> ids;
    ids.reserve(g_active_mods.size());
    for (auto const& [id, _] : g_active_mods)
        ids.push_back(id);
    for (const auto& id : ids)
        deactivate_internal(id);
    g_active_mods.clear();
}

bool activate_mod(const std::string& id) {
    auto current = get_active_mod_ids();
    if (std::find(current.begin(), current.end(), id) == current.end())
        current.push_back(id);
    return set_active_mods(current);
}

bool deactivate_mod(const std::string& id) {
    auto current = get_active_mod_ids();
    auto it = std::remove(current.begin(), current.end(), id);
    if (it == current.end())
        return false;
    current.erase(it, current.end());
    return set_active_mods(current);
}

bool reload_mod(const std::string& id) {
    return reload_mods({id});
}

bool reload_mods(const std::vector<std::string>& ids) {
    if (ids.empty())
        return false;
    bool changed = false;
    for (const auto& id : ids) {
        if (deactivate_internal(id))
            changed = true;
    }
    if (changed)
        rebuild_mod_assets();
    for (const auto& id : ids) {
        if (auto* info = find_mod_info_mutable(id)) {
            if (!info->enabled)
                info->enabled = true;
            if (activate_internal(*info))
                changed = true;
        }
    }
    return changed;
}

bool set_active_mods(const std::vector<std::string>& ids) {
    if (!mm)
        return false;
    std::unordered_set<std::string> desired(ids.begin(), ids.end());
    bool changed = false;

    std::vector<std::string> to_remove;
    to_remove.reserve(g_active_mods.size());
    for (const auto& [id, _] : g_active_mods) {
        if (!desired.count(id))
            to_remove.push_back(id);
    }
    for (const auto& id : to_remove) {
        if (deactivate_internal(id))
            changed = true;
    }

    for (auto& mod : mm->mods) {
        bool enable = desired.count(mod.name) > 0;
        if (mod.enabled != enable) {
            mod.enabled = enable;
            changed = true;
        }
    }

    if (changed)
        rebuild_mod_assets();

    for (auto& mod : mm->mods) {
        if (!desired.count(mod.name))
            continue;
        if (!g_active_mods.count(mod.name)) {
            if (activate_internal(mod))
                changed = true;
        }
    }

    return changed;
}

std::vector<std::string> get_active_mod_ids() {
    std::vector<std::string> ids;
    if (!mm) {
        ids.reserve(g_active_mods.size());
        for (const auto& [id, _] : g_active_mods)
            ids.push_back(id);
        return ids;
    }
    for (const auto& mod : mm->mods) {
        if (g_active_mods.count(mod.name))
            ids.push_back(mod.name);
    }
    return ids;
}

const std::unordered_map<std::string, ModContext>& active_mod_contexts() {
    return g_active_mods;
}
