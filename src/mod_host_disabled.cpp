#include "src/mod_host.hpp"

#include <cstdio>

namespace {

std::unordered_map<std::string, ModContext> g_empty_active_mods;
std::string g_required_version;

void print_disabled_once() {
    static bool printed = false;
    if (printed)
        return;
    printed = true;
    std::fprintf(stderr, "[mod_host] Lua mod host disabled at build time.\n");
}

} // namespace

void set_required_mod_game_version(const std::string& version) {
    g_required_version = version;
}

const std::string& required_mod_game_version() {
    return g_required_version;
}

bool load_enabled_mods_via_host(EngineState&) {
    print_disabled_once();
    return false;
}

bool reload_all_mods_via_host(EngineState&) {
    print_disabled_once();
    return false;
}

void unload_all_mods_via_host(EngineState&) {
}

bool activate_mod(EngineState&, const std::string&) {
    print_disabled_once();
    return false;
}

bool deactivate_mod(EngineState&, const std::string&) {
    return false;
}

bool reload_mod(EngineState&, const std::string&) {
    print_disabled_once();
    return false;
}

bool reload_mods(EngineState&, const std::vector<std::string>&) {
    print_disabled_once();
    return false;
}

bool set_active_mods(EngineState&, const std::vector<std::string>&) {
    print_disabled_once();
    return false;
}

std::vector<std::string> get_active_mod_ids(EngineState&) {
    return {};
}

const std::unordered_map<std::string, ModContext>& active_mod_contexts() {
    return g_empty_active_mods;
}
