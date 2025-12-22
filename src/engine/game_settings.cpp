#include "engine/game_settings.hpp"

#include "engine/globals.hpp"
#include "engine/parser.hpp"
#include "engine/utils.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <unordered_set>

namespace {

constexpr const char* kGameSettingsPath = "data/settings_profiles/game_settings.sxp";

// Global schema storage
GameSettingsSchema g_game_settings_schema;

std::vector<GameSettings> parse_game_settings_tree(const std::vector<sexp::SValue>& roots) {
    const sexp::SValue* root = nullptr;
    for (const auto& node : roots) {
        if (node.type != sexp::SValue::Type::List || node.list.empty())
            continue;
        if (sexp::is_symbol(node.list.front(), "game_settings_list")) {
            root = &node;
            break;
        }
    }
    std::vector<GameSettings> settings_list;
    if (!root)
        return settings_list;

    for (size_t i = 1; i < root->list.size(); ++i) {
        const sexp::SValue& entry = root->list[i];
        if (entry.type != sexp::SValue::Type::List || entry.list.empty())
            continue;
        if (!sexp::is_symbol(entry.list.front(), "settings"))
            continue;

        auto id = sexp::extract_int(entry, "id");
        auto name = sexp::extract_string(entry, "name");
        if (!id || !name)
            continue;

        GameSettings settings{};
        settings.id = *id;
        settings.name = *name;

        // Parse key-value pairs
        const sexp::SValue* values_node = sexp::find_child(entry, "values");
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

                std::string key = kv.list[1].text;
                const sexp::SValue& value = kv.list[2];

                // Determine type and store
                if (value.type == sexp::SValue::Type::Int) {
                    settings.settings[key] = static_cast<int>(value.int_value);
                } else if (value.type == sexp::SValue::Type::Float) {
                    settings.settings[key] = static_cast<float>(value.float_value);
                } else if (value.type == sexp::SValue::Type::String) {
                    settings.settings[key] = value.text;
                } else if (value.type == sexp::SValue::Type::List && value.list.size() >= 3 &&
                           sexp::is_symbol(value.list[0], "vec2")) {
                    // Format: (vec2 x y)
                    float x = 0.0f, y = 0.0f;
                    const sexp::SValue& x_val = value.list[1];
                    const sexp::SValue& y_val = value.list[2];

                    if (x_val.type == sexp::SValue::Type::Float)
                        x = static_cast<float>(x_val.float_value);
                    else if (x_val.type == sexp::SValue::Type::Int)
                        x = static_cast<float>(x_val.int_value);

                    if (y_val.type == sexp::SValue::Type::Float)
                        y = static_cast<float>(y_val.float_value);
                    else if (y_val.type == sexp::SValue::Type::Int)
                        y = static_cast<float>(y_val.int_value);

                    settings.settings[key] = GameSettingsVec2{x, y};
                }
            }
        }

        settings_list.push_back(std::move(settings));
    }
    return settings_list;
}

std::vector<GameSettings> read_game_settings_from_disk() {
    std::ifstream f(kGameSettingsPath);
    if (!f.is_open())
        return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    auto parsed = sexp::parse_s_expressions(oss.str());
    if (!parsed) {
        std::fprintf(stderr, "[game_settings] Failed to parse %s\n", kGameSettingsPath);
        return {};
    }
    return parse_game_settings_tree(*parsed);
}

bool write_game_settings_file(const std::vector<GameSettings>& settings_list) {
    namespace fs = std::filesystem;
    fs::path path(kGameSettingsPath);
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
        out << "    (name " << sexp::quote_string(settings.name) << ")\n";
        out << "    (values\n";

        for (const auto& [key, value] : settings.settings) {
            out << "      (key " << sexp::quote_string(key) << " ";

            std::visit([&out](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int>) {
                    out << arg;
                } else if constexpr (std::is_same_v<T, float>) {
                    out << arg;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    out << sexp::quote_string(arg);
                } else if constexpr (std::is_same_v<T, GameSettingsVec2>) {
                    out << "(vec2 " << arg.x << " " << arg.y << ")";
                }
            }, value);

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

bool load_game_settings_pool() {
    if (!es)
        return false;
    es->game_settings_pool = load_all_game_settings();
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

void set_game_setting_string(GameSettings& settings, const std::string& key, const std::string& value) {
    settings.settings[key] = value;
}

void set_game_setting_vec2(GameSettings& settings, const std::string& key, float x, float y) {
    settings.settings[key] = GameSettingsVec2{x, y};
}

int get_game_setting_int(const GameSettings& settings, const std::string& key, int default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const int* p = std::get_if<int>(&it->second))
        return *p;
    return default_value;
}

float get_game_setting_float(const GameSettings& settings, const std::string& key, float default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const float* p = std::get_if<float>(&it->second))
        return *p;
    return default_value;
}

std::string get_game_setting_string(const GameSettings& settings, const std::string& key, const std::string& default_value) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return default_value;
    if (const std::string* p = std::get_if<std::string>(&it->second))
        return *p;
    return default_value;
}

GameSettingsVec2 get_game_setting_vec2(const GameSettings& settings, const std::string& key, float default_x, float default_y) {
    auto it = settings.settings.find(key);
    if (it == settings.settings.end())
        return GameSettingsVec2{default_x, default_y};
    if (const GameSettingsVec2* p = std::get_if<GameSettingsVec2>(&it->second))
        return *p;
    return GameSettingsVec2{default_x, default_y};
}

void GameSettingsSchema::add_int(const std::string& key, int default_value) {
    entries.push_back({key, default_value});
}

void GameSettingsSchema::add_float(const std::string& key, float default_value) {
    entries.push_back({key, default_value});
}

void GameSettingsSchema::add_string(const std::string& key, const std::string& default_value) {
    entries.push_back({key, default_value});
}

void GameSettingsSchema::add_vec2(const std::string& key, float default_x, float default_y) {
    entries.push_back({key, GameSettingsVec2{default_x, default_y}});
}

void register_game_settings_schema(const GameSettingsSchema& schema) {
    // Store the schema globally
    g_game_settings_schema = schema;

    // Load all existing game settings profiles
    auto all_settings = load_all_game_settings();

    // Reconcile each profile with the new schema
    for (auto& settings : all_settings) {
        std::unordered_map<std::string, GameSettingsValue> reconciled;

        for (const auto& entry : schema.entries) {
            auto it = settings.settings.find(entry.key);
            if (it != settings.settings.end()) {
                // Key exists - check if types match
                if (it->second.index() == entry.default_value.index()) {
                    // Types match - keep the existing value
                    reconciled[entry.key] = it->second;
                } else {
                    // Type mismatch - use default value from schema
                    reconciled[entry.key] = entry.default_value;
                }
            } else {
                // Key doesn't exist - use default value from schema
                reconciled[entry.key] = entry.default_value;
            }
        }

        settings.settings = reconciled;
        save_game_settings(settings);
    }

    // Reload into engine state
    load_game_settings_pool();
}

GameSettings create_game_settings_from_schema() {
    GameSettings settings;
    settings.id = generate_game_settings_id();
    settings.name = "NewGameSettings";

    // Populate with defaults from schema
    for (const auto& entry : g_game_settings_schema.entries) {
        settings.settings[entry.key] = entry.default_value;
    }

    save_game_settings(settings);
    return settings;
}
