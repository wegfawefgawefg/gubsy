#include <gubsy/app.hpp>
#include <gubsy/input/binds.hpp>
#include <gubsy/layout/layout.hpp>
#include <gubsy/lobby/session.hpp>
#include <gubsy/menu/menu.hpp>
#include <gubsy/profiles/profiles.hpp>
#include <gubsy/run.hpp>
#include <gubsy/settings/settings.hpp>

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
