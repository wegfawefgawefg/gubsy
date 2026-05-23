#include "src/game_settings.hpp"

#include "src/engine_state.hpp"
#include "src/project_paths.hpp"
#include "src/settings_schema.hpp"
#include "src/sexp_helpers.hpp"
#include "src/utils.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <unordered_set>
#include <vector>

namespace {

std::vector<GameSettings> parse_game_settings_tree(const gsexp::ParseResult& parsed) {
    gsexp::Node root = gubsy_sexp::find_root(parsed, "game_settings_list");
    std::vector<GameSettings> settings_list;
    if (!root.valid())
        return settings_list;

    for (std::size_t i = 1; i < root.child_count(); ++i) {
        gsexp::Node entry = root.child_at(i);
        if (!entry.is_list())
            continue;
        if (!entry.head().is_atom("settings"))
            continue;

        gsexp::FormView entry_view(entry);
        auto id = gubsy_sexp::field_to_int(entry_view, "id");
        auto name = gubsy_sexp::field_to_string(entry_view, "name");
        if (!id || !name)
            continue;

        GameSettings settings{};
        settings.id = *id;
        settings.name = *name;

        gsexp::Node values_node = entry_view.find("values");
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

        settings_list.push_back(std::move(settings));
    }
    return settings_list;
}

std::vector<GameSettings> read_game_settings_from_disk() {
    std::filesystem::path path = data_path("settings_profiles/game_settings.lisp");
    auto text = gubsy_sexp::read_text_file(path);
    if (!text)
        return {};
    gsexp::ParseResult parsed = gsexp::parse_owned(std::move(*text));
    if (!parsed.ok) {
        std::fprintf(stderr, "[game_settings] Failed to parse %s\n", path.string().c_str());
        return {};
    }
    return parse_game_settings_tree(parsed);
}

bool write_game_settings_file(const std::vector<GameSettings>& settings_list) {
    namespace fs = std::filesystem;
    fs::path path = data_path("settings_profiles/game_settings.lisp");
    if (path.has_parent_path()) {
        if (!ensure_dir(path.parent_path().string()))
            return false;
    }
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << "(game_settings_list\n";
    for (const auto& settings : settings_list) {
        out << "  (settings\n";
        out << "    (id " << settings.id << ")\n";
        out << "    (name " << gsexp::quote_string(settings.name) << ")\n";
        out << "    (values\n";

        std::vector<std::string> keys;
        keys.reserve(settings.settings.size());
        for (const auto& [key, value] : settings.settings)
            keys.push_back(key);
        std::sort(keys.begin(), keys.end());

        for (const std::string& key : keys) {
            const SettingsValue& value = settings.settings.at(key);
            out << "      (key " << gsexp::quote_string(key) << " ";

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

        out << "    )\n";
        out << "  )\n";
    }
    out << ")\n";
    return out.good();
}

} // namespace

std::vector<GameSettings> load_all_game_settings() {
    return read_game_settings_from_disk();
}

GameSettings load_game_settings(int settings_id) {
    auto settings_list = read_game_settings_from_disk();
    auto it = std::find_if(settings_list.begin(), settings_list.end(),
                           [&](const GameSettings& s) { return s.id == settings_id; });
    if (it != settings_list.end())
        return *it;
    GameSettings empty{};
    empty.id = -1;
    return empty;
}

bool save_game_settings(const GameSettings& settings) {
    if (settings.id <= 0)
        return false;
    if (settings.name.empty())
        return false;

    auto settings_list = read_game_settings_from_disk();

    // Prevent duplicate names (except for same settings)
    for (const auto& existing : settings_list) {
        if (existing.id != settings.id && existing.name == settings.name)
            return false;
    }

    // Update existing or add new
    bool updated = false;
    for (auto& existing : settings_list) {
        if (existing.id == settings.id) {
            existing = settings;
            updated = true;
            break;
        }
    }
    if (!updated) {
        // Check for duplicate ID
        for (const auto& existing : settings_list) {
            if (existing.id == settings.id)
                return false;
        }
        settings_list.push_back(settings);
    }
    return write_game_settings_file(settings_list);
}

bool load_game_settings_pool(EngineState& engine) {
    engine.game_settings_pool = load_all_game_settings();
    return true;
}

int generate_game_settings_id() {
    std::unordered_set<int> used;
    auto settings_list = load_all_game_settings();
    used.reserve(settings_list.size());
    for (const auto& settings : settings_list)
        used.insert(settings.id);
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(10000000, 99999999);
    for (int attempt = 0; attempt < 4096; ++attempt) {
        int candidate = dist(rng);
        if (!used.count(candidate))
            return candidate;
    }
    return dist(rng);
}

GameSettings create_default_game_settings() {
    GameSettings settings;
    settings.id = generate_game_settings_id();
    settings.name = "DefaultGameSettings";
    // Empty settings map - developer can add their custom settings
    save_game_settings(settings);
    return settings;
}

void set_game_setting_int(GameSettings& settings, const std::string& key, int value) {
    settings.settings[key] = value;
}

void set_game_setting_float(GameSettings& settings, const std::string& key, float value) {
    settings.settings[key] = value;
}

void set_game_setting_string(GameSettings& settings, const std::string& key,
                             const std::string& value) {
    settings.settings[key] = value;
}

void set_game_setting_vec2(GameSettings& settings, const std::string& key, float x, float y) {
    settings.settings[key] = SettingsVec2{x, y};
}

int get_game_setting_int(const GameSettings& settings, const std::string& key, int default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const int* p = std::get_if<int>(&it->second))
        return *p;
    return default_value;
}

float get_game_setting_float(const GameSettings& settings, const std::string& key,
                             float default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const float* p = std::get_if<float>(&it->second))
        return *p;
    return default_value;
}

std::string get_game_setting_string(const GameSettings& settings, const std::string& key,
                                    const std::string& default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const std::string* p = std::get_if<std::string>(&it->second))
        return *p;
    return default_value;
}

SettingsVec2 get_game_setting_vec2(const GameSettings& settings, const std::string& key,
                                   float default_x, float default_y) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return SettingsVec2{default_x, default_y};
    if (const SettingsVec2* p = std::get_if<SettingsVec2>(&it->second))
        return *p;
    return SettingsVec2{default_x, default_y};
}

GameSettings create_game_settings_from_schema() {
    GameSettings settings;
    settings.id = generate_game_settings_id();
    settings.name = "NewGameSettings";

    const auto& schema = get_settings_schema();
    for (const auto& entry : schema.entries()) {
        if (entry.scope != SettingScope::Profile)
            continue;
        settings.settings[entry.key] = entry.default_value;
    }

    save_game_settings(settings);
    return settings;
}
