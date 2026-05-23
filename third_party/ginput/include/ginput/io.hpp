#pragma once

#include "ginput/profile.hpp"
#include "ginput/reconcile.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ginput {

struct Diagnostic {
    std::string message;
    int line = 1;
    int column = 1;
};

struct LoadProfilesResult {
    bool ok = false;
    std::vector<InputProfile> profiles;
    ReconcileReport reconcile_report;
    std::vector<Diagnostic> diagnostics;
};

LoadProfilesResult load_profiles_string(std::string_view text,
                                        std::string_view root_name = "input_profiles");
LoadProfilesResult load_profiles_string(std::string_view text, const Schema& schema,
                                        std::string_view root_name = "input_profiles");
LoadProfilesResult load_profiles_file(const std::filesystem::path& path,
                                      std::string_view root_name = "input_profiles");
LoadProfilesResult load_profiles_file(const std::filesystem::path& path, const Schema& schema,
                                      std::string_view root_name = "input_profiles");

bool save_profiles_string(const std::vector<InputProfile>& profiles, std::string& out,
                          std::string_view root_name = "input_profiles");
bool save_profiles_file(const std::filesystem::path& path,
                        const std::vector<InputProfile>& profiles,
                        std::string_view root_name = "input_profiles");

} // namespace ginput
