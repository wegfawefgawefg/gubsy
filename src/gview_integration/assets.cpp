#include "gubsy/ui/gview.hpp"
#include "src/project_paths.hpp"

namespace gubsy::ui {

// Resolves GView asset IDs through the same configured roots as the rest of Gubsy.
std::filesystem::path resolve_view_asset(AssetDomain domain,
                                         const std::filesystem::path& relative) {
    switch (domain) {
    case AssetDomain::Game:
        return game_assets_path(relative);
    case AssetDomain::Engine:
        return engine_assets_path(relative);
    case AssetDomain::Data:
        return data_path(relative);
    case AssetDomain::RuntimeMods:
        return runtime_mods_path(relative);
    }
    return relative;
}

} // namespace gubsy::ui
