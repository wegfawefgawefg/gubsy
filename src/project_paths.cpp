#include "src/project_paths.hpp"

#include "gubsy/app.hpp"

#include <cstdlib>
#include <optional>

namespace fs = std::filesystem;

namespace {

fs::path normalize_path(const fs::path& path) {
    std::error_code ec;
    fs::path normalized = fs::weakly_canonical(path, ec);
    if (!ec)
        return normalized;
    return path.lexically_normal();
}

fs::path append_relative(const fs::path& root, const fs::path& relative) {
    if (relative.empty())
        return root;
    return normalize_path(root / relative);
}

std::optional<fs::path> config_path(const std::string& path) {
    if (path.empty())
        return std::nullopt;
    return normalize_path(fs::path(path));
}

fs::path detect_project_root() {
    if (const char* override_root = std::getenv("GUB_PROJECT_ROOT")) {
        if (*override_root != '\0')
            return normalize_path(fs::path(override_root));
    }
    return normalize_path(fs::path(GUB_PROJECT_SOURCE_DIR));
}

struct ProjectPathConfig {
    fs::path project_root{detect_project_root()};
    std::optional<fs::path> data_root;
    std::optional<fs::path> game_root;
    std::optional<fs::path> tools_root;
    std::optional<fs::path> engine_assets_root;
};

ProjectPathConfig& project_path_config() {
    static ProjectPathConfig config;
    return config;
}

} // namespace

void configure_project_paths(const GubsyAppConfig& config) {
    ProjectPathConfig& paths = project_path_config();
    if (!config.project_root.empty()) {
        paths.project_root = normalize_path(fs::path(config.project_root));
    } else {
        paths.project_root = detect_project_root();
    }
    paths.data_root = config_path(config.data_root);
    paths.game_root = config_path(config.game_root);
    paths.tools_root = config_path(config.tools_root);
    paths.engine_assets_root = config_path(config.engine_assets_root);
}

fs::path project_root() {
    return project_path_config().project_root;
}

fs::path engine_root() {
    return append_relative(project_root(), "src");
}

fs::path game_root() {
    if (project_path_config().game_root)
        return *project_path_config().game_root;
    return append_relative(project_root(), "demo");
}

fs::path tools_root() {
    if (project_path_config().tools_root)
        return *project_path_config().tools_root;
    return append_relative(project_root(), "tools");
}

fs::path data_root() {
    if (project_path_config().data_root)
        return *project_path_config().data_root;
    return append_relative(project_root(), "data");
}

fs::path engine_assets_path(const fs::path& relative) {
    if (project_path_config().engine_assets_root)
        return append_relative(*project_path_config().engine_assets_root, relative);
    return append_relative(engine_root() / "assets", relative);
}

fs::path game_assets_path(const fs::path& relative) {
    return append_relative(game_root() / "assets", relative);
}

fs::path game_mods_path(const fs::path& relative) {
    return append_relative(game_root() / "mods", relative);
}

fs::path runtime_mods_path(const fs::path& relative) {
    return append_relative(data_root() / "mods", relative);
}

fs::path mod_repo_path(const fs::path& relative) {
    return append_relative(tools_root() / "mod_repo", relative);
}

fs::path data_path(const fs::path& relative) {
    return append_relative(data_root(), relative);
}
