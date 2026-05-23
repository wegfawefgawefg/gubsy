#include "engine/user_profiles.hpp"

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

std::vector<UserProfile> parse_profiles_tree(const gsexp::ParseResult& parsed) {
    gsexp::Node root = gubsy_sexp::find_root(parsed, "user_profiles");
    std::vector<UserProfile> profiles;
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
        UserProfile profile{};
        profile.id = *id;
        profile.name = *name;
        profile.last_binds_profile_id =
            gubsy_sexp::field_to_int(entry_view, "last_binds_profile").value_or(-1);
        profile.last_input_settings_profile_id =
            gubsy_sexp::field_to_int(entry_view, "last_input_settings_profile_id").value_or(-1);
        profile.last_game_settings_profile_id =
            gubsy_sexp::field_to_int(entry_view, "last_game_settings").value_or(-1);
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::vector<UserProfile> read_profiles_from_disk() {
    std::filesystem::path path = data_path("player_profiles/user_profiles.lisp");
    auto text = gubsy_sexp::read_text_file(path);
    if (!text)
        return {};
    gsexp::ParseResult parsed = gsexp::parse_owned(std::move(*text));
    if (!parsed.ok) {
        std::fprintf(stderr, "[profiles] Failed to parse %s\n", path.string().c_str());
        return {};
    }
    return parse_profiles_tree(parsed);
}

bool write_profiles_file(const std::vector<UserProfile>& profiles) {
    namespace fs = std::filesystem;
    fs::path path = data_path("player_profiles/user_profiles.lisp");
    if (path.has_parent_path()) {
        if (!ensure_dir(path.parent_path().string()))
            return false;
    }
    std::ofstream out(path);
    if (!out.is_open())
        return false;
    out << "(user_profiles\n";
    for (const auto& profile : profiles) {
        out << "  (profile\n";
        out << "    (id " << profile.id << ")\n";
        out << "    (name " << gsexp::quote_string(profile.name) << ")\n";
        out << "    (last_binds_profile " << profile.last_binds_profile_id << ")\n";
        out << "    (last_input_settings_profile_id " << profile.last_input_settings_profile_id
            << ")\n";
        out << "    (last_game_settings " << profile.last_game_settings_profile_id << ")\n";
        out << "  )\n";
    }
    out << ")\n";
    return out.good();
}

} // namespace

std::vector<UserProfile> load_all_user_profile_metadatas() {
    return read_profiles_from_disk();
}

UserProfile load_user_profile(int profile_id) {
    auto profiles = read_profiles_from_disk();
    auto it = std::find_if(profiles.begin(), profiles.end(),
                           [&](const UserProfile& profile) { return profile.id == profile_id; });
    if (it != profiles.end())
        return *it;
    UserProfile empty{};
    empty.id = -1;
    empty.last_binds_profile_id = -1;
    empty.last_input_settings_profile_id = -1;
    empty.last_game_settings_profile_id = -1;
    return empty;
}

bool save_user_profile(const UserProfile& profile) {
    if (profile.id <= 0)
        return false;
    if (profile.name.empty())
        return false;
    auto profiles = read_profiles_from_disk();
    for (const auto& existing : profiles) {
        if (existing.id != profile.id && existing.name == profile.name)
            return false;
    }
    bool updated = false;
    for (auto& existing : profiles) {
        if (existing.id == profile.id) {
            existing = profile;
            updated = true;
            break;
        }
    }
    if (!updated) {
        for (const auto& existing : profiles) {
            if (existing.id == profile.id)
                return false;
        }
        profiles.push_back(profile);
    }
    return write_profiles_file(profiles);
}

bool delete_user_profile(int profile_id) {
    if (profile_id <= 0)
        return false;
    auto profiles = read_profiles_from_disk();
    auto it = std::remove_if(profiles.begin(), profiles.end(),
                             [&](const UserProfile& profile) { return profile.id == profile_id; });
    if (it == profiles.end())
        return false;
    profiles.erase(it, profiles.end());
    return write_profiles_file(profiles);
}

bool load_user_profiles_pool(EngineState& engine) {
    engine.user_profiles_pool = load_all_user_profile_metadatas();
    return true;
}

int generate_user_profile_id() {
    std::unordered_set<int> used;
    auto profiles = load_all_user_profile_metadatas();
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

UserProfile create_default_user_profile() {
    UserProfile profile;
    profile.id = generate_user_profile_id();
    profile.name = "DefaultProfile";
    profile.last_binds_profile_id = -1;
    profile.last_input_settings_profile_id = -1;
    profile.last_game_settings_profile_id = -1;
    save_user_profile(profile);
    return profile;
}

std::vector<UserProfile>& get_user_profiles_pool(EngineState& engine) {
    return engine.user_profiles_pool;
}
