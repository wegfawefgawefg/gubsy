#include "engine/binds_profiles.hpp"

#include "engine/engine_state.hpp"
#include "engine/project_paths.hpp"
#include "engine/sexp_helpers.hpp"
#include "engine/utils.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <unordered_set>

namespace {

static BindsSchema g_schema;

std::vector<BindsProfile> parse_binds_profiles_tree(const gsexp::ParseResult& parsed) {
    gsexp::Node root = gubsy_sexp::find_root(parsed, "binds_profiles");
    std::vector<BindsProfile> profiles;
    if (!root.valid())
        return profiles;

    for (std::size_t i = 1; i < root.child_count(); ++i) {
        gsexp::Node entry = root.child_at(i);
        if (!entry.is_list())
            continue;
        if (!entry.head().is_atom("profile"))
            continue;

        gsexp::FormView entry_view(entry);
        auto id = gubsy_sexp::field_to_int(entry_view, "id");
        auto name = gubsy_sexp::field_to_string(entry_view, "name");
        if (!id || !name)
            continue;

        BindsProfile profile{};
        profile.id = *id;
        profile.name = *name;

        gsexp::Node button_binds_node = entry_view.find("button_binds");
        if (button_binds_node.valid()) {
            for (std::size_t j = 1; j < button_binds_node.child_count(); ++j) {
                gsexp::Node bind = button_binds_node.child_at(j);
                if (!bind.is_list() || !bind.head().is_atom("bind"))
                    continue;
                gsexp::FormView bind_view(bind);
                auto device_btn = gubsy_sexp::field_to_int(bind_view, "device_button");
                auto gubsy_act = gubsy_sexp::field_to_int(bind_view, "gubsy_action");
                if (device_btn && gubsy_act) {
                    profile.button_binds.push_back({*device_btn, *gubsy_act});
                }
            }
        }

        gsexp::Node analog_1d_node = entry_view.find("analog_1d_binds");
        if (analog_1d_node.valid()) {
            for (std::size_t j = 1; j < analog_1d_node.child_count(); ++j) {
                gsexp::Node bind = analog_1d_node.child_at(j);
                if (!bind.is_list() || !bind.head().is_atom("bind"))
                    continue;
                gsexp::FormView bind_view(bind);
                auto device_axis = gubsy_sexp::field_to_int(bind_view, "device_axis");
                auto gubsy_analog = gubsy_sexp::field_to_int(bind_view, "gubsy_analog");
                if (device_axis && gubsy_analog) {
                    profile.analog_1d_binds.push_back({*device_axis, *gubsy_analog});
                }
            }
        }

        gsexp::Node analog_2d_node = entry_view.find("analog_2d_binds");
        if (analog_2d_node.valid()) {
            for (std::size_t j = 1; j < analog_2d_node.child_count(); ++j) {
                gsexp::Node bind = analog_2d_node.child_at(j);
                if (!bind.is_list() || !bind.head().is_atom("bind"))
                    continue;
                gsexp::FormView bind_view(bind);
                auto device_stick = gubsy_sexp::field_to_int(bind_view, "device_stick");
                auto gubsy_stick = gubsy_sexp::field_to_int(bind_view, "gubsy_stick");
                if (device_stick && gubsy_stick) {
                    profile.analog_2d_binds.push_back({*device_stick, *gubsy_stick});
                }
            }
        }

        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::vector<BindsProfile> read_binds_profiles_from_disk() {
    std::filesystem::path path = data_path("binds_profiles/default.lisp");
    auto text = gubsy_sexp::read_text_file(path);
    if (!text)
        return {};
    gsexp::ParseResult parsed = gsexp::parse_owned(std::move(*text));
    if (!parsed.ok) {
        std::fprintf(stderr, "[binds] Failed to parse %s\n", path.string().c_str());
        return {};
    }
    return parse_binds_profiles_tree(parsed);
}

bool write_binds_profiles_file(const std::vector<BindsProfile>& profiles) {
    namespace fs = std::filesystem;
    fs::path path = data_path("binds_profiles/default.lisp");
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
        out << "    (name " << gsexp::quote_string(profile.name) << ")\n";

        // Write button_binds
        out << "    (button_binds\n";
        for (const auto& [device_btn, gubsy_act] : profile.button_binds) {
            out << "      (bind (device_button " << device_btn << ") (gubsy_action " << gubsy_act
                << "))\n";
        }
        out << "    )\n";

        // Write analog_1d_binds
        out << "    (analog_1d_binds\n";
        for (const auto& [device_axis, gubsy_analog] : profile.analog_1d_binds) {
            out << "      (bind (device_axis " << device_axis << ") (gubsy_analog " << gubsy_analog
                << "))\n";
        }
        out << "    )\n";

        // Write analog_2d_binds
        out << "    (analog_2d_binds\n";
        for (const auto& [device_stick, gubsy_stick] : profile.analog_2d_binds) {
            out << "      (bind (device_stick " << device_stick << ") (gubsy_stick " << gubsy_stick
                << "))\n";
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

bool load_binds_profiles_pool(EngineState& engine) {
    engine.binds_profiles = load_all_binds_profiles();
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
    const auto new_bind = std::make_pair(device_button, gubsy_action);
    for (const auto& existing_bind : profile.button_binds) {
        if (existing_bind == new_bind) {
            return; // Already exists
        }
    }
    profile.button_binds.push_back(new_bind);
}

void bind_1d_analog(BindsProfile& profile, int device_axis, int gubsy_1d_analog) {
    const auto new_bind = std::make_pair(device_axis, gubsy_1d_analog);
    for (const auto& existing_bind : profile.analog_1d_binds) {
        if (existing_bind == new_bind) {
            return; // Already exists
        }
    }
    profile.analog_1d_binds.push_back(new_bind);
}

void bind_2d_analog(BindsProfile& profile, int device_stick, int gubsy_2d_analog) {
    const auto new_bind = std::make_pair(device_stick, gubsy_2d_analog);
    for (const auto& existing_bind : profile.analog_2d_binds) {
        if (existing_bind == new_bind) {
            return; // Already exists
        }
    }
    profile.analog_2d_binds.push_back(new_bind);
}

std::vector<BindsProfile>& get_binds_profiles_pool(EngineState& engine) {
    return engine.binds_profiles;
}

void bind_button(BindsProfile& profile, GubsyButton device_button, int gubsy_action) {
    bind_button(profile, static_cast<int>(device_button), gubsy_action);
}

void bind_1d_analog(BindsProfile& profile, Gubsy1DAnalog device_axis, int gubsy_1d_analog) {
    bind_1d_analog(profile, static_cast<int>(device_axis), gubsy_1d_analog);
}

void bind_2d_analog(BindsProfile& profile, Gubsy2DAnalog device_stick, int gubsy_2d_analog) {
    bind_2d_analog(profile, static_cast<int>(device_stick), gubsy_2d_analog);
}

void BindsSchema::add_action(int action_id, const std::string& display_name,
                             const std::string& category) {
    actions.push_back({action_id, display_name, category});
}

void BindsSchema::add_1d_analog(int analog_id, const std::string& display_name,
                                const std::string& category) {
    analogs_1d.push_back({analog_id, display_name, category});
}

void BindsSchema::add_2d_analog(int analog_id, const std::string& display_name,
                                const std::string& category) {
    analogs_2d.push_back({analog_id, display_name, category});
}

void register_binds_schema(EngineState& engine, const BindsSchema& schema) {
    g_schema = schema;
    // Build sets of valid IDs from schema
    std::unordered_set<int> valid_actions;
    valid_actions.reserve(schema.actions.size());
    for (const auto& entry : schema.actions) {
        valid_actions.insert(entry.action_id);
    }

    std::unordered_set<int> valid_analogs_1d;
    valid_analogs_1d.reserve(schema.analogs_1d.size());
    for (const auto& entry : schema.analogs_1d) {
        valid_analogs_1d.insert(entry.analog_id);
    }

    std::unordered_set<int> valid_analogs_2d;
    valid_analogs_2d.reserve(schema.analogs_2d.size());
    for (const auto& entry : schema.analogs_2d) {
        valid_analogs_2d.insert(entry.analog_id);
    }

    // Load all existing binds profiles
    auto all_profiles = load_all_binds_profiles();

    // Reconcile each profile with the new schema
    for (auto& profile : all_profiles) {
        // Filter button_binds to keep only valid actions
        std::vector<std::pair<int, int>> valid_button_binds;
        for (const auto& [device_btn, gubsy_action] : profile.button_binds) {
            if (valid_actions.count(gubsy_action)) {
                valid_button_binds.push_back({device_btn, gubsy_action});
            }
        }
        profile.button_binds = std::move(valid_button_binds);

        // Filter analog_1d_binds to keep only valid 1D analogs
        std::vector<std::pair<int, int>> valid_analog_1d_binds;
        for (const auto& [device_axis, gubsy_analog] : profile.analog_1d_binds) {
            if (valid_analogs_1d.count(gubsy_analog)) {
                valid_analog_1d_binds.push_back({device_axis, gubsy_analog});
            }
        }
        profile.analog_1d_binds = std::move(valid_analog_1d_binds);

        // Filter analog_2d_binds to keep only valid 2D analogs
        std::vector<std::pair<int, int>> valid_analog_2d_binds;
        for (const auto& [device_stick, gubsy_stick] : profile.analog_2d_binds) {
            if (valid_analogs_2d.count(gubsy_stick)) {
                valid_analog_2d_binds.push_back({device_stick, gubsy_stick});
            }
        }
        profile.analog_2d_binds = std::move(valid_analog_2d_binds);

        save_binds_profile(profile);
    }

    // Reload into engine state
    load_binds_profiles_pool(engine);
}

const BindsSchema& get_binds_schema() {
    return g_schema;
}
