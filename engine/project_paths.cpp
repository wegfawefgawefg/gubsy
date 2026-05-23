#include "engine/project_paths.hpp"

#include <cstdlib>

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

fs::path detect_project_root() {
    if (const char* override_root = std::getenv("GUB_PROJECT_ROOT")) {
        if (*override_root != '\0')
            return normalize_path(fs::path(override_root));
    }
    return normalize_path(fs::path(GUB_PROJECT_SOURCE_DIR));
}

} // namespace

fs::path project_root() {
    static const fs::path root = detect_project_root();
    return root;
}

fs::path engine_root() {
    return append_relative(project_root(), "engine");
}

fs::path game_root() {
    return append_relative(project_root(), "demo");
}

fs::path tools_root() {
    return append_relative(project_root(), "tools");
}

fs::path data_root() {
    return append_relative(project_root(), "data");
}

fs::path engine_assets_path(const fs::path& relative) {
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
