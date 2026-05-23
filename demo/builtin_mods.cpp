#include "demo/builtin_mods.hpp"

#include "src/project_paths.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace {

bool copy_tree(const fs::path& src, const fs::path& dst, std::string& err) {
    std::error_code ec;
    fs::create_directories(dst, ec);
    if (ec) {
        err = "Failed to create directory: " + dst.string();
        return false;
    }

    for (const auto& entry : fs::recursive_directory_iterator(src, ec)) {
        if (ec) {
            err = "Failed to read built-in mod tree: " + src.string();
            return false;
        }

        fs::path rel = fs::relative(entry.path(), src, ec);
        if (ec) {
            err = "Failed to compute relative path in " + src.string();
            return false;
        }

        fs::path target = dst / rel;
        if (entry.is_directory()) {
            fs::create_directories(target, ec);
            if (ec) {
                err = "Failed to create directory: " + target.string();
                return false;
            }
            continue;
        }

        if (!entry.is_regular_file())
            continue;

        fs::create_directories(target.parent_path(), ec);
        if (ec) {
            err = "Failed to create directory: " + target.parent_path().string();
            return false;
        }

        fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            err = "Failed to copy built-in mod file: " + target.string();
            return false;
        }
    }

    return true;
}

bool should_refresh_copy(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    if (!fs::exists(dst, ec) || !fs::is_directory(dst, ec))
        return true;

    fs::path src_manifest = src / "manifest.json";
    fs::path dst_manifest = dst / "manifest.json";
    if (!fs::exists(src_manifest, ec))
        return false;
    if (!fs::exists(dst_manifest, ec))
        return true;

    auto src_time = fs::last_write_time(src_manifest, ec);
    if (ec)
        return false;
    auto dst_time = fs::last_write_time(dst_manifest, ec);
    if (ec)
        return true;
    return src_time > dst_time;
}

} // namespace

bool sync_builtin_game_mods(std::string& err) {
    std::error_code ec;
    fs::path shipped_root = game_mods_path();
    fs::path runtime_root = runtime_mods_path();

    fs::create_directories(runtime_root, ec);
    if (ec) {
        err = "Failed to prepare runtime mods directory: " + runtime_root.string();
        return false;
    }

    if (!fs::exists(shipped_root, ec) || !fs::is_directory(shipped_root, ec))
        return true;

    for (const auto& entry : fs::directory_iterator(shipped_root, ec)) {
        if (ec) {
            err = "Failed to scan built-in mods in " + shipped_root.string();
            return false;
        }
        if (!entry.is_directory())
            continue;

        fs::path src = entry.path();
        fs::path dst = runtime_root / src.filename();
        if (!should_refresh_copy(src, dst))
            continue;

        fs::remove_all(dst, ec);
        ec.clear();
        if (!copy_tree(src, dst, err))
            return false;
    }

    return true;
}
