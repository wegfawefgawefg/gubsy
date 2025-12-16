#include "engine/top_level_game_settings.hpp"

#include "engine/config.hpp"
#include "engine/globals.hpp"
#include "engine/parser.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

constexpr const char* kTopLevelSettingsPath = "config/top_level_game_settings.sxp";

TopLevelGameSettings parse_top_level_settings_tree(const std::vector<sexp::SValue>& roots) {
    const sexp::SValue* root = nullptr;
    for (const auto& node : roots) {
        if (node.type != sexp::SValue::Type::List || node.list.empty())
            continue;
        if (sexp::is_symbol(node.list.front(), "top_level_game_settings")) {
            root = &node;
            break;
        }
    }

    TopLevelGameSettings settings{};
    if (!root)
        return settings;

    // Parse key-value pairs
    const sexp::SValue* values_node = sexp::find_child(*root, "values");
    if (values_node && values_node->type == sexp::SValue::Type::List) {
        for (size_t j = 1; j < values_node->list.size(); ++j) {
            const sexp::SValue& kv = values_node->list[j];
            if (kv.type != sexp::SValue::Type::List || kv.list.size() < 3)
                continue;

            // Format: (key "key_name" value)
            if (!sexp::is_symbol(kv.list[0], "key"))
                continue;

            if (kv.list[1].type != sexp::SValue::Type::String)
                continue;

            std::string key = kv.list[1].string_value;
            const sexp::SValue& value = kv.list[2];

            // Determine type and store
            if (value.type == sexp::SValue::Type::Int) {
                settings.settings[key] = static_cast<int>(value.int_value);
            } else if (value.type == sexp::SValue::Type::Float) {
                settings.settings[key] = static_cast<float>(value.float_value);
            } else if (value.type == sexp::SValue::Type::String) {
                settings.settings[key] = value.string_value;
            }
        }
    }

    return settings;
}

TopLevelGameSettings read_top_level_settings_from_disk() {
    std::ifstream f(kTopLevelSettingsPath);
    if (!f.is_open())
        return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    auto parsed = sexp::parse_s_expressions(oss.str());
    if (!parsed) {
        std::fprintf(stderr, "[top_level_settings] Failed to parse %s\n", kTopLevelSettingsPath);
        return {};
    }
    return parse_top_level_settings_tree(*parsed);
}

bool write_top_level_settings_file(const TopLevelGameSettings& settings) {
    namespace fs = std::filesystem;
    fs::path path(kTopLevelSettingsPath);
    if (path.has_parent_path()) {
        if (!ensure_dir(path.parent_path().string()))
            return false;
    }
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << "(top_level_game_settings\n";
    out << "  (values\n";

    for (const auto& [key, value] : settings.settings) {
        out << "    (key " << sexp::quote_string(key) << " ";

        std::visit([&out](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, float>) {
                out << arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
                out << sexp::quote_string(arg);
            }
        }, value);

        out << ")\n";
    }

    out << "  )\n";
    out << ")\n";
    return out.good();
}

} // namespace

TopLevelGameSettings load_top_level_game_settings() {
    return read_top_level_settings_from_disk();
}

bool save_top_level_game_settings(const TopLevelGameSettings& settings) {
    return write_top_level_settings_file(settings);
}

bool load_top_level_game_settings_into_state() {
    if (!es)
        return false;
    es->top_level_game_settings = load_top_level_game_settings();
    return true;
}

void set_top_level_setting_int(TopLevelGameSettings& settings, const std::string& key, int value) {
    settings.settings[key] = value;
}

void set_top_level_setting_float(TopLevelGameSettings& settings, const std::string& key, float value) {
    settings.settings[key] = value;
}

void set_top_level_setting_string(TopLevelGameSettings& settings, const std::string& key, const std::string& value) {
    settings.settings[key] = value;
}

int get_top_level_setting_int(const TopLevelGameSettings& settings, const std::string& key, int default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const int* p = std::get_if<int>(&it->second))
        return *p;
    return default_value;
}

float get_top_level_setting_float(const TopLevelGameSettings& settings, const std::string& key, float default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const float* p = std::get_if<float>(&it->second))
        return *p;
    return default_value;
}

std::string get_top_level_setting_string(const TopLevelGameSettings& settings, const std::string& key, const std::string& default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const std::string* p = std::get_if<std::string>(&it->second))
        return *p;
    return default_value;
}
