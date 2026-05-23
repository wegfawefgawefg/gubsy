#include "game/mod_api/demo_content_api.hpp"

#include "engine/mod_api_registry.hpp"
#include "engine/mod_host.hpp"
#include "game/mod_api/demo_content_internal.hpp"

#include <sol/sol.hpp>

void register_demo_content_api(ModApiRegistry& registry) {
    ModApiDescriptor desc;
    desc.name = "demo_content";
    desc.bind_lua = [](sol::state& lua, ModContext& ctx) {
        lua.set_function("register_demo_content", [&ctx](const sol::table& tbl) {
            demo_content_internal::remove_override(ctx.id);
            demo_content_internal::register_override(ctx.id, tbl);
        });
        auto ignore_legacy_def = [](const sol::table&) {};
        lua.set_function("register_powerup", ignore_legacy_def);
        lua.set_function("register_projectile", ignore_legacy_def);
        lua.set_function("register_ammo", ignore_legacy_def);
        lua.set_function("register_gun", ignore_legacy_def);
        lua.set_function("register_entity_type", ignore_legacy_def);
        lua.set_function("register_crate", ignore_legacy_def);
    };
    desc.on_mod_unloaded = [](ModContext& ctx) {
        demo_content_internal::remove_override(ctx.id);
    };
    registry.register_api(std::move(desc));
}

void finalize_demo_content_from_mods(State& state) {
    demo_content_internal::apply_overrides(state);
}
