#include <gubsy/app.hpp>
#include <gubsy/menu/menu.hpp>
#include <gubsy/run.hpp>
#include <gubsy/runtime.hpp>

int main() {
    GubsyRuntime runtime{};
    GubsyAppHooks hooks{};

    if (hooks.config.enable_mods ||
        hooks.config.enable_mod_browser ||
        hooks.config.enable_mod_hot_reload ||
        hooks.config.enable_lua_mod_host) {
        return 1;
    }

    if (!init_engine_state(runtime, hooks.config)) {
        return 2;
    }

    bool mods_registered = runtime.menu_manager.find_screen(MenuScreenID::MODS) != nullptr;
    cleanup_engine_state(runtime);
    return mods_registered ? 3 : 0;
}
