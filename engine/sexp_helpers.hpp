#pragma once

#include "engine/settings_types.hpp"

#include <filesystem>
#include <gsexp/sexp.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace gubsy_sexp {

std::optional<std::string> read_text_file(const std::filesystem::path& path);

gsexp::Node find_root(const gsexp::ParseResult& parsed, std::string_view head);
std::optional<int> field_to_int(gsexp::FormView form, std::string_view head);
std::optional<float> field_to_float(gsexp::FormView form, std::string_view head);
std::optional<std::string> field_to_string(gsexp::FormView form, std::string_view head);
std::optional<int> node_to_int(gsexp::Node node);
std::optional<float> node_to_float(gsexp::Node node);
std::optional<std::string> node_to_string(gsexp::Node node);
std::optional<SettingsValue> node_to_settings_value(gsexp::Node node);

} // namespace gubsy_sexp
