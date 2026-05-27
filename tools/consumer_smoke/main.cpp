#include <gubsy/app.hpp>

int main() {
    GubsyAppHooks hooks{};

    if (hooks.config.enable_mods ||
        hooks.config.enable_mod_browser ||
        hooks.config.enable_mod_hot_reload ||
        hooks.config.enable_lua_mod_host) {
        return 1;
    }

    return 0;
}
