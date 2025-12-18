#include "game/setup.hpp"

#include "engine/globals.hpp"
#include "engine/mod_host.hpp"
#include "engine/render.hpp"
#include "engine/mod_install.hpp"
#include "engine/mods.hpp"
#include "game/modes.hpp"
#include "game/mod_api/register_game_mod_apis.hpp"

#include <SDL2/SDL.h>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr const char* kModServerUrl = "http://127.0.0.1:8787";
const std::vector<std::string> kDemoModChain = {
    "base",
    "patch_core",
    "bonus_pack",
    "pad_tweaks",
    "shuffle_pack",
    "super_suite",
};

enum class SetupState {
    Idle,
    Running,
    Failed,
    Ready
};

SetupState g_state = SetupState::Idle;
std::string g_status = "Preparing mods...";
std::string g_error;

std::filesystem::path mods_root_dir() {
    if (mm && !mm->root.empty())
        return std::filesystem::path(mm->root);
    return std::filesystem::path("mods_runtime");
}

bool clear_mod_cache(std::string& err) {
    std::error_code ec;
    auto root = mods_root_dir();
    std::filesystem::remove_all(root, ec);
    ec.clear();
    if (!std::filesystem::create_directories(root, ec) && ec) {
        err = "Failed to prepare mods directory: " + root.string();
        return false;
    }
    return true;
}

bool ensure_demo_mods_installed(std::string& err) {
    std::vector<ModCatalogEntry> catalog;
    g_status = "Fetching mod catalog...";
    if (!fetch_mod_catalog(kModServerUrl, catalog, err))
        return false;

    auto find_entry = [&](const std::string& id) -> const ModCatalogEntry* {
        for (const auto& entry : catalog) {
            if (entry.id == id)
                return &entry;
        }
        return nullptr;
    };

    for (const auto& id : kDemoModChain) {
        const ModCatalogEntry* entry = find_entry(id);
        if (!entry) {
            err = "Catalog missing mod '" + id + "'";
            return false;
        }
        std::string folder = entry->folder.empty() ? entry->id : entry->folder;
        auto path = mods_root_dir() / folder;
        std::error_code ec;
        bool installed = std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
        if (installed)
            continue;

        g_status = "Installing " + entry->title + "...";
        if (!install_mod_from_catalog(kModServerUrl, *entry, err))
            return false;
    }
    return true;
}

bool run_setup_once() {
    std::string err;
    g_status = "Clearing local mod cache...";
    if (!clear_mod_cache(err)) {
        g_error = err;
        return false;
    }
    if (!ensure_demo_mods_installed(err)) {
        g_error = err;
        return false;
    }
    g_status = "Discovering mods...";
    if (!mm) {
        g_error = "Mod manager unavailable";
        return false;
    }
    discover_mods();
    g_status = "Activating mods...";
    if (!set_active_mods(kDemoModChain)) {
        g_error = "Failed to activate mods";
        return false;
    }
    g_status = "Mods ready";
    return true;
}

void finalize_and_enter_play() {
    finalize_game_mod_apis();
    es->mode = modes::PLAYING;
}

} // namespace

void setup_step() {
    if (!es)
        return;

    switch (g_state) {
    case SetupState::Ready:
        finalize_and_enter_play();
        return;
    case SetupState::Failed:
        return;
    case SetupState::Idle:
        g_state = SetupState::Running;
        if (run_setup_once())
            g_state = SetupState::Ready;
        else
            g_state = SetupState::Failed;
        return;
    case SetupState::Running:
    default:
        return;
    }
}

void setup_draw() {
    if (!gg || !gg->renderer)
        return;
    SDL_Renderer* renderer = gg->renderer;
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_SetRenderDrawColor(renderer, 10, 8, 16, 255);
    SDL_RenderClear(renderer);

    if (g_state == SetupState::Failed) {
        draw_text(renderer, "Mod setup failed", 40, height / 2 - 20, SDL_Color{255, 120, 120, 255});
        draw_text(renderer, g_error.empty() ? "Check mod server status."
                                            : g_error,
                  40, height / 2 + 10, SDL_Color{200, 150, 150, 255});
    } else {
        draw_text(renderer, "Preparing mods...", 40, height / 2 - 20,
                  SDL_Color{200, 200, 210, 255});
        draw_text(renderer, g_status, 40, height / 2 + 10,
                  SDL_Color{180, 180, 200, 255});
    }

}
