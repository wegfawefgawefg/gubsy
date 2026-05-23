#pragma once

#include <filesystem>

struct GubsyAppConfig;

void configure_project_paths(const GubsyAppConfig& config);

std::filesystem::path project_root();
std::filesystem::path engine_root();
std::filesystem::path game_root();
std::filesystem::path tools_root();
std::filesystem::path data_root();

std::filesystem::path engine_assets_path(const std::filesystem::path& relative = {});
std::filesystem::path game_assets_path(const std::filesystem::path& relative = {});
std::filesystem::path game_mods_path(const std::filesystem::path& relative = {});
std::filesystem::path runtime_mods_path(const std::filesystem::path& relative = {});
std::filesystem::path mod_repo_path(const std::filesystem::path& relative = {});
std::filesystem::path data_path(const std::filesystem::path& relative = {});
