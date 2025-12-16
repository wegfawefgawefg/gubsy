#include "engine/binds_profiles.hpp"

#include "engine/config.hpp"
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

constexpr const char* kBindsProfilesPath = "config/binds_profiles.sxp";

std::vector<BindsProfile> parse_binds_profiles_tree(const std::vector<sexp::SValue>& roots) {
    const sexp::SValue* root = nullptr;
    for (const auto& node : roots) {
        if (node.type != sexp::SValue::Type::List || node.list.empty())
            continue;
        if (sexp::is_symbol(node.list.front(), "binds_profiles")) {
            root = &node;
            break;
        }
    }
    std::vector<BindsProfile> profiles;
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

        BindsProfile profile{};
        profile.id = *id;
        profile.name = *name;

        // Parse button_binds
        const sexp::SValue* button_binds_node = sexp::find_child(entry, "button_binds");
        if (button_binds_node && button_binds_node->type == sexp::SValue::Type::List) {
            for (size_t j = 1; j < button_binds_node->list.size(); ++j) {
                const sexp::SValue& bind = button_binds_node->list[j];
                if (bind.type == sexp::SValue::Type::List && bind.list.size() >= 3) {
                    if (sexp::is_symbol(bind.list[0], "bind")) {
                        auto device_btn = sexp::extract_int(bind, "device_button");
                        auto gubsy_act = sexp::extract_int(bind, "gubsy_action");
                        if (device_btn && gubsy_act) {
                            profile.button_binds[*device_btn] = *gubsy_act;
                        }
                    }
                }
            }
        }

        // Parse analog_1d_binds
        const sexp::SValue* analog_1d_node = sexp::find_child(entry, "analog_1d_binds");
        if (analog_1d_node && analog_1d_node->type == sexp::SValue::Type::List) {
            for (size_t j = 1; j < analog_1d_node->list.size(); ++j) {
                const sexp::SValue& bind = analog_1d_node->list[j];
                if (bind.type == sexp::SValue::Type::List && bind.list.size() >= 3) {
                    if (sexp::is_symbol(bind.list[0], "bind")) {
                        auto device_axis = sexp::extract_int(bind, "device_axis");
                        auto gubsy_analog = sexp::extract_int(bind, "gubsy_analog");
                        if (device_axis && gubsy_analog) {
                            profile.analog_1d_binds[*device_axis] = *gubsy_analog;
                        }
                    }
                }
            }
        }

        // Parse analog_2d_binds
        const sexp::SValue* analog_2d_node = sexp::find_child(entry, "analog_2d_binds");
        if (analog_2d_node && analog_2d_node->type == sexp::SValue::Type::List) {
            for (size_t j = 1; j < analog_2d_node->list.size(); ++j) {
                const sexp::SValue& bind = analog_2d_node->list[j];
                if (bind.type == sexp::SValue::Type::List && bind.list.size() >= 3) {
                    if (sexp::is_symbol(bind.list[0], "bind")) {
                        auto device_stick = sexp::extract_int(bind, "device_stick");
                        auto gubsy_stick = sexp::extract_int(bind, "gubsy_stick");
                        if (device_stick && gubsy_stick) {
                            profile.analog_2d_binds[*device_stick] = *gubsy_stick;
                        }
                    }
                }
            }
        }

        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::vector<BindsProfile> read_binds_profiles_from_disk() {
    std::ifstream f(kBindsProfilesPath);
    if (!f.is_open())
        return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    auto parsed = sexp::parse_s_expressions(oss.str());
    if (!parsed) {
        std::fprintf(stderr, "[binds] Failed to parse %s\n", kBindsProfilesPath);
        return {};
    }
    return parse_binds_profiles_tree(*parsed);
}

bool write_binds_profiles_file(const std::vector<BindsProfile>& profiles) {
    namespace fs = std::filesystem;
    fs::path path(kBindsProfilesPath);
    if (path.has_parent_path()) {
        if (!ensure_dir(path.parent_path().string()))
            return false;
    }
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << "(binds_profiles\n";
    for (const auto& profile : profiles) {
        out << "  (profile\n";
        out << "    (id " << profile.id << ")\n";
        out << "    (name " << sexp::quote_string(profile.name) << ")\n";

        // Write button_binds
        out << "    (button_binds\n";
        for (const auto& [device_btn, gubsy_act] : profile.button_binds) {
            out << "      (bind (device_button " << device_btn << ") (gubsy_action " << gubsy_act << "))\n";
        }
        out << "    )\n";

        // Write analog_1d_binds
        out << "    (analog_1d_binds\n";
        for (const auto& [device_axis, gubsy_analog] : profile.analog_1d_binds) {
            out << "      (bind (device_axis " << device_axis << ") (gubsy_analog " << gubsy_analog << "))\n";
        }
        out << "    )\n";

        // Write analog_2d_binds
        out << "    (analog_2d_binds\n";
        for (const auto& [device_stick, gubsy_stick] : profile.analog_2d_binds) {
            out << "      (bind (device_stick " << device_stick << ") (gubsy_stick " << gubsy_stick << "))\n";
        }
        out << "    )\n";

        out << "  )\n";
    }
    out << ")\n";
    return out.good();
}

} // namespace

std::vector<BindsProfile> load_all_binds_profiles() {
    return read_binds_profiles_from_disk();
}

BindsProfile load_binds_profile(int profile_id) {
    auto profiles = read_binds_profiles_from_disk();
    auto it = std::find_if(profiles.begin(), profiles.end(),
                          [&](const BindsProfile& profile) { return profile.id == profile_id; });
    if (it != profiles.end())
        return *it;
    BindsProfile empty{};
    empty.id = -1;
    return empty;
}

bool save_binds_profile(const BindsProfile& profile) {
    if (profile.id <= 0)
        return false;
    if (profile.name.empty())
        return false;

    auto profiles = read_binds_profiles_from_disk();

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
    return write_binds_profiles_file(profiles);
}

bool load_binds_profiles_pool() {
    if (!es)
        return false;
    es->binds_profiles = load_all_binds_profiles();
    return true;
}

int generate_binds_profile_id() {
    std::unordered_set<int> used;
    auto profiles = load_all_binds_profiles();
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

BindsProfile create_default_binds_profile() {
    BindsProfile profile;
    profile.id = generate_binds_profile_id();
    profile.name = "DefaultBinds";
    // Empty binds - developer can add them in their game
    save_binds_profile(profile);
    return profile;
}

void bind_button(BindsProfile& profile, int device_button, int gubsy_action) {
    profile.button_binds[device_button] = gubsy_action;
}

void bind_1d_analog(BindsProfile& profile, int device_axis, int gubsy_1d_analog) {
    profile.analog_1d_binds[device_axis] = gubsy_1d_analog;
}

void bind_2d_analog(BindsProfile& profile, int device_stick, int gubsy_2d_analog) {
    profile.analog_2d_binds[device_stick] = gubsy_2d_analog;
}
