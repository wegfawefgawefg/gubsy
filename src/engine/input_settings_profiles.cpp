#include "engine/input_settings_profiles.hpp"

#include "engine/utils.hpp"
#include "engine/globals.hpp"
#include "engine/parser.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <unordered_set>

namespace {

constexpr const char* kInputSettingsProfilesPath = "data/settings_profiles/input_settings_profiles.lisp";

// Helper to extract bool (treat non-zero int as true)
std::optional<bool> extract_bool(const sexp::SValue& list, const std::string& symbol) {
    auto val = sexp::extract_int(list, symbol);
    if (!val)
        return std::nullopt;
    return *val != 0;
}

std::vector<InputSettingsProfile> parse_input_settings_profiles_tree(const std::vector<sexp::SValue>& roots) {
    const sexp::SValue* root = nullptr;
    for (const auto& node : roots) {
        if (node.type != sexp::SValue::Type::List || node.list.empty())
            continue;
        if (sexp::is_symbol(node.list.front(), "input_settings_profiles")) {
            root = &node;
            break;
        }
    }
    std::vector<InputSettingsProfile> profiles;
    if (!root)
        return profiles;

    for (size_t i = 1; i < root->list.size(); ++i) {
        const sexp::SValue& entry = root->list[i];
        if (entry.type != sexp::SValue::Type::List || entry.list.empty())
            continue;
        if (!sexp::is_symbol(entry.list.front(), "profile"))
            continue;

        auto id = sexp::extract_int(entry, "id");
        auto name = sexp::extract_string(entry, "name");
        if (!id || !name)
            continue;

        InputSettingsProfile profile{};
        profile.id = *id;
        profile.name = *name;
        profile.mouse_sensitivity = sexp::extract_float(entry, "mouse_sensitivity").value_or(1.0f);
        profile.mouse_invert_x = extract_bool(entry, "mouse_invert_x").value_or(false);
        profile.mouse_invert_y = extract_bool(entry, "mouse_invert_y").value_or(false);
        profile.controller_sensitivity = sexp::extract_float(entry, "controller_sensitivity").value_or(1.0f);
        profile.stick_deadzone = sexp::extract_float(entry, "stick_deadzone").value_or(0.15f);
        profile.trigger_threshold = sexp::extract_float(entry, "trigger_threshold").value_or(0.1f);
        profile.controller_invert_x = extract_bool(entry, "controller_invert_x").value_or(false);
        profile.controller_invert_y = extract_bool(entry, "controller_invert_y").value_or(false);
        profile.vibration_enabled = extract_bool(entry, "vibration_enabled").value_or(true);
        profile.vibration_strength = sexp::extract_float(entry, "vibration_strength").value_or(1.0f);

        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::vector<InputSettingsProfile> read_input_settings_profiles_from_disk() {
    std::ifstream f(kInputSettingsProfilesPath);
    if (!f.is_open())
        return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    auto parsed = sexp::parse_s_expressions(oss.str());
    if (!parsed) {
        std::fprintf(stderr, "[input_settings] Failed to parse %s\n", kInputSettingsProfilesPath);
        return {};
    }
    return parse_input_settings_profiles_tree(*parsed);
}

bool write_input_settings_profiles_file(const std::vector<InputSettingsProfile>& profiles) {
    namespace fs = std::filesystem;
    fs::path path(kInputSettingsProfilesPath);
    if (path.has_parent_path()) {
        if (!ensure_dir(path.parent_path().string()))
            return false;
    }
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << "(input_settings_profiles\n";
    for (const auto& profile : profiles) {
        out << "  (profile\n";
        out << "    (id " << profile.id << ")\n";
        out << "    (name " << sexp::quote_string(profile.name) << ")\n";
        out << "    (mouse_sensitivity " << profile.mouse_sensitivity << ")\n";
        out << "    (mouse_invert_x " << (profile.mouse_invert_x ? 1 : 0) << ")\n";
        out << "    (mouse_invert_y " << (profile.mouse_invert_y ? 1 : 0) << ")\n";
        out << "    (controller_sensitivity " << profile.controller_sensitivity << ")\n";
        out << "    (stick_deadzone " << profile.stick_deadzone << ")\n";
        out << "    (trigger_threshold " << profile.trigger_threshold << ")\n";
        out << "    (controller_invert_x " << (profile.controller_invert_x ? 1 : 0) << ")\n";
        out << "    (controller_invert_y " << (profile.controller_invert_y ? 1 : 0) << ")\n";
        out << "    (vibration_enabled " << (profile.vibration_enabled ? 1 : 0) << ")\n";
        out << "    (vibration_strength " << profile.vibration_strength << ")\n";
        out << "  )\n";
    }
    out << ")\n";
    return out.good();
}

} // namespace

std::vector<InputSettingsProfile> load_all_input_settings_profiles() {
    return read_input_settings_profiles_from_disk();
}

InputSettingsProfile load_input_settings_profile(int profile_id) {
    auto profiles = read_input_settings_profiles_from_disk();
    auto it = std::find_if(profiles.begin(), profiles.end(),
                          [&](const InputSettingsProfile& profile) { return profile.id == profile_id; });
    if (it != profiles.end())
        return *it;
    InputSettingsProfile empty{};
    empty.id = -1;
    return empty;
}

bool save_input_settings_profile(const InputSettingsProfile& profile) {
    if (profile.id <= 0)
        return false;
    if (profile.name.empty())
        return false;

    auto profiles = read_input_settings_profiles_from_disk();

    // Prevent duplicate names (except for same profile)
    for (const auto& existing : profiles) {
        if (existing.id != profile.id && existing.name == profile.name)
            return false;
    }

    // Update existing or add new
    bool updated = false;
    for (auto& existing : profiles) {
        if (existing.id == profile.id) {
            existing = profile;
            updated = true;
            break;
        }
    }
    if (!updated) {
        // Check for duplicate ID
        for (const auto& existing : profiles) {
            if (existing.id == profile.id)
                return false;
        }
        profiles.push_back(profile);
    }
    return write_input_settings_profiles_file(profiles);
}

bool load_input_settings_profiles_pool() {
    if (!es)
        return false;
    es->input_settings_profiles = load_all_input_settings_profiles();
    return true;
}

int generate_input_settings_profile_id() {
    std::unordered_set<int> used;
    auto profiles = load_all_input_settings_profiles();
    used.reserve(profiles.size());
    for (const auto& profile : profiles)
        used.insert(profile.id);
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(10000000, 99999999);
    for (int attempt = 0; attempt < 4096; ++attempt) {
        int candidate = dist(rng);
        if (!used.count(candidate))
            return candidate;
    }
    return dist(rng);
}

InputSettingsProfile create_default_input_settings_profile() {
    InputSettingsProfile profile;
    profile.id = generate_input_settings_profile_id();
    profile.name = "DefaultInputSettings";
    profile.mouse_sensitivity = 1.0f;
    profile.mouse_invert_x = false;
    profile.mouse_invert_y = false;
    profile.controller_sensitivity = 1.0f;
    profile.stick_deadzone = 0.15f;
    profile.trigger_threshold = 0.1f;
    profile.controller_invert_x = false;
    profile.controller_invert_y = false;
    profile.vibration_enabled = true;
    profile.vibration_strength = 1.0f;
    save_input_settings_profile(profile);
    return profile;
}
