#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sol {
class state;
}

struct ModApiDescriptor;

struct ModContext {
    std::string id;
    std::string title;
    std::string path;
    std::vector<std::string> requested_apis;
    std::vector<const ModApiDescriptor*> bound_apis;
    std::unique_ptr<sol::state> lua;
};

void set_required_mod_game_version(const std::string& version);
const std::string& required_mod_game_version();

bool load_enabled_mods_via_host();
bool reload_all_mods_via_host();
void unload_all_mods_via_host();
bool activate_mod(const std::string& id);
bool deactivate_mod(const std::string& id);
bool reload_mod(const std::string& id);
bool reload_mods(const std::vector<std::string>& ids);
bool set_active_mods(const std::vector<std::string>& ids);
std::vector<std::string> get_active_mod_ids();

const std::unordered_map<std::string, ModContext>& active_mod_contexts();
