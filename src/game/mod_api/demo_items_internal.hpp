#pragma once

#include <string>
#include <sol/sol.hpp>

namespace demo_items_internal {

void expose_runtime_api(sol::state& lua);
void register_item(const std::string& mod_id, const sol::table& def);
bool patch_item(const std::string& mod_id, const sol::table& patch);
void remove_mod_items(const std::string& mod_id);
void finalize_items();
bool has_active_items();

} // namespace demo_items_internal
