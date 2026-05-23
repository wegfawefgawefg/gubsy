#include <gubsy/app.hpp>
#include <gubsy/menu/ids.hpp>
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

    if (!init_gubsy_runtime(runtime, hooks.config)) {
        return 2;
    }

    bool mods_registered = gubsy_runtime_has_menu_screen(runtime, MenuScreenID::MODS);
    cleanup_gubsy_runtime(runtime);
    return mods_registered ? 3 : 0;
}
