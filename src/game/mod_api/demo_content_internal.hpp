#pragma once

#include <string>
#include <sol/sol.hpp>

namespace demo_content_internal {

void register_override(const std::string& mod_id, const sol::table& tbl);
void remove_override(const std::string& mod_id);
void apply_overrides();

} // namespace demo_content_internal
