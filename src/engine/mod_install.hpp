#pragma once

#include "engine/mods.hpp"

#include <string>
#include <vector>

bool fetch_mod_catalog(const std::string& server_url,
                       std::vector<ModCatalogEntry>& out,
                       std::string& err);

bool install_mod_from_catalog(const std::string& server_url,
                              const ModCatalogEntry& entry,
                              std::string& err);

bool uninstall_mod(const ModCatalogEntry& entry, std::string& err);
