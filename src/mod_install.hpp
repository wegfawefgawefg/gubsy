#pragma once

#include "src/mods.hpp"

#include <string>
#include <vector>

struct EngineState;

bool fetch_mod_catalog(const std::string& server_url,
                       std::vector<ModCatalogEntry>& out,
                       std::string& err);

bool install_mod_from_catalog(EngineState& engine,
                              const std::string& server_url,
                              const ModCatalogEntry& entry,
                              std::string& err);

bool uninstall_mod(EngineState& engine, const ModCatalogEntry& entry, std::string& err);

bool sync_mod_selection_from_catalog(EngineState& engine,
                                     const std::string& server_url,
                                     const std::vector<std::string>& required_ids,
                                     std::string& err);
