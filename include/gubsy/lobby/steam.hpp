#pragma once

struct GubsySteamBackendStatus {
    bool compiled{false};
    bool sdk_ready{false};
    const char* message{""};
};

GubsySteamBackendStatus gubsy_steam_backend_status();
