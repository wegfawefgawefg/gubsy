#include "gubsy/lobby/steam.hpp"

GubsySteamBackendStatus gubsy_steam_backend_status() {
#if GUB_ENABLE_STEAM
    return {
        true,
        false,
        "Steam backend scaffold compiled; Steamworks SDK wiring is not installed",
    };
#else
    return {
        false,
        false,
        "Steam backend disabled at compile time",
    };
#endif
}
