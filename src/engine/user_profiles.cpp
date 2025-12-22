#include "engine/user_profiles.hpp"

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

constexpr const char* kUserProfilesPath = "data/player_profiles/user_profiles.lisp";

std::vector<UserProfile> parse_profiles_tree(const std::vector<sexp::SValue>& roots) {
    const sexp::SValue* root = nullptr;
    for (const auto& node : roots) {
        if (node.type != sexp::SValue::Type::List || node.list.empty())
            continue;
        if (sexp::is_symbol(node.list.front(), "user_profiles")) {
            root = &node;
            break;
        }
    }
    std::vector<UserProfile> profiles;
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
        UserProfile profile{};
        profile.id = *id;
        profile.name = *name;
        profile.last_binds_profile_id = sexp::extract_int(entry, "last_binds_profile").value_or(-1);
        profile.last_input_settings_profile_id = sexp::extract_int(entry, "last_input_settings_profile_id").value_or(-1);
        profile.last_game_settings_profile_id = sexp::extract_int(entry, "last_game_settings").value_or(-1);
        profiles.push_back(std::move(profile));
    }
    return profiles;
}

std::vector<UserProfile> read_profiles_from_disk() {
    std::ifstream f(kUserProfilesPath);
    if (!f.is_open())
        return {};
    std::ostringstream oss;
    oss << f.rdbuf();
    auto parsed = sexp::parse_s_expressions(oss.str());
    if (!parsed) {
        std::fprintf(stderr, "[profiles] Failed to parse %s\n", kUserProfilesPath);
        return {};
    }
    return parse_profiles_tree(*parsed);
}

bool write_profiles_file(const std::vector<UserProfile>& profiles) {
    namespace fs = std::filesystem;
    fs::path path(kUserProfilesPath);
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
        out << "    (name " << sexp::quote_string(profile.name) << ")\n";
        out << "    (last_binds_profile " << profile.last_binds_profile_id << ")\n";
        out << "    (last_input_settings_profile_id " << profile.last_input_settings_profile_id << ")\n";
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

bool load_user_profiles_pool() {
    if (!es)
        return false;
    es->user_profiles_pool = load_all_user_profile_metadatas();
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

std::vector<UserProfile>& get_user_profiles_pool() {
    return es->user_profiles_pool;
}
