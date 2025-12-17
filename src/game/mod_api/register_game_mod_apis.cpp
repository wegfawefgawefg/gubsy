#include "game/mod_api/register_game_mod_apis.hpp"

#include "engine/mod_api_registry.hpp"
#include "game/mod_api/demo_content_api.hpp"
#include "game/mod_api/demo_items_api.hpp"

void register_game_mod_apis() {
    auto& registry = ModApiRegistry::instance();
    register_demo_content_api(registry);
    register_demo_items_api(registry);
}

void finalize_game_mod_apis() {
    finalize_demo_content_from_mods();
    finalize_demo_items_from_mods();
}
