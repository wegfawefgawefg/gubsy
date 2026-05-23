#include "src/top_level_game_settings.hpp"

#include "src/engine_state.hpp"
#include "src/project_paths.hpp"
#include "src/sexp_helpers.hpp"
#include "src/utils.hpp"

#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

TopLevelGameSettings parse_top_level_settings_tree(const gsexp::ParseResult& parsed) {
    gsexp::Node root = gubsy_sexp::find_root(parsed, "top_level_game_settings");

    TopLevelGameSettings settings{};
    if (!root.valid())
        return settings;

    gsexp::Node values_node = gsexp::FormView(root).find("values");
    if (values_node.valid()) {
        for (std::size_t j = 1; j < values_node.child_count(); ++j) {
            gsexp::Node kv = values_node.child_at(j);
            if (!kv.is_list() || kv.child_count() < 3)
                continue;
            if (!kv.head().is_atom("key"))
                continue;

            gsexp::Node key_node = kv.child_at(1);
            if (!key_node.is_string())
                continue;

            auto value = gubsy_sexp::node_to_settings_value(kv.child_at(2));
            if (!value)
                continue;

            settings.settings[std::string(key_node.text())] = std::move(*value);
        }
    }

    return settings;
}

TopLevelGameSettings read_top_level_settings_from_disk() {
    std::filesystem::path path = data_path("settings_profiles/top_level_game_settings.lisp");
    auto text = gubsy_sexp::read_text_file(path);
    if (!text)
        return {};
    gsexp::ParseResult parsed = gsexp::parse_owned(std::move(*text));
    if (!parsed.ok) {
        std::fprintf(stderr, "[top_level_settings] Failed to parse %s\n", path.string().c_str());
        return {};
    }
    return parse_top_level_settings_tree(parsed);
}

bool write_top_level_settings_file(const TopLevelGameSettings& settings) {
    namespace fs = std::filesystem;
    fs::path path = data_path("settings_profiles/top_level_game_settings.lisp");
    if (path.has_parent_path()) {
        if (!ensure_dir(path.parent_path().string()))
            return false;
    }
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << "(top_level_game_settings\n";
    out << "  (values\n";

    std::vector<std::string> keys;
    keys.reserve(settings.settings.size());
    for (const auto& [key, value] : settings.settings)
        keys.push_back(key);
    std::sort(keys.begin(), keys.end());

    for (const std::string& key : keys) {
        const SettingsValue& value = settings.settings.at(key);
        out << "    (key " << gsexp::quote_string(key) << " ";

        std::visit(
            [&out](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int>) {
                    out << arg;
                } else if constexpr (std::is_same_v<T, float>) {
                    out << arg;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    out << gsexp::quote_string(arg);
                } else if constexpr (std::is_same_v<T, SettingsVec2>) {
                    out << "(vec2 " << arg.x << " " << arg.y << ")";
                }
            },
            value);

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

bool load_top_level_game_settings_into_state(EngineState& engine) {
    engine.top_level_game_settings = load_top_level_game_settings();
    return true;
}

void set_top_level_setting_int(TopLevelGameSettings& settings, const std::string& key, int value) {
    settings.settings[key] = value;
}

void set_top_level_setting_float(TopLevelGameSettings& settings, const std::string& key,
                                 float value) {
    settings.settings[key] = value;
}

void set_top_level_setting_string(TopLevelGameSettings& settings, const std::string& key,
                                  const std::string& value) {
    settings.settings[key] = value;
}

int get_top_level_setting_int(const TopLevelGameSettings& settings, const std::string& key,
                              int default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const int* p = std::get_if<int>(&it->second))
        return *p;
    return default_value;
}

float get_top_level_setting_float(const TopLevelGameSettings& settings, const std::string& key,
                                  float default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const float* p = std::get_if<float>(&it->second))
        return *p;
    return default_value;
}

std::string get_top_level_setting_string(const TopLevelGameSettings& settings,
                                         const std::string& key, const std::string& default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const std::string* p = std::get_if<std::string>(&it->second))
        return *p;
    return default_value;
}
