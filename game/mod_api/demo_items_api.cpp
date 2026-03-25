#include "game/mod_api/demo_items_api.hpp"

#include "engine/mod_api_registry.hpp"
#include "engine/mod_host.hpp"
#include "game/mod_api/demo_items_internal.hpp"

#include <sol/sol.hpp>

void register_demo_items_api(ModApiRegistry& registry) {
    ModApiDescriptor desc;
    desc.name = "demo_items";
    desc.bind_lua = [](sol::state& lua, ModContext& ctx) {
        demo_items_internal::expose_runtime_api(lua);
        lua.set_function("register_item", [&ctx](const sol::table& def) {
            demo_items_internal::register_item(ctx.id, def);
        });
        lua.set_function("patch_item", [&ctx](const sol::table& patch) {
            demo_items_internal::patch_item(ctx.id, patch);
        });
    };
    desc.on_mod_unloaded = [](ModContext& ctx) {
        demo_items_internal::remove_mod_items(ctx.id);
    };
    registry.register_api(std::move(desc));
}

void finalize_demo_items_from_mods() {
    demo_items_internal::finalize_items();
}
