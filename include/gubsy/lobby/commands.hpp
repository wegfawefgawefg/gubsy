#pragma once

#include "gubsy/lobby/state.hpp"

#include <cstdint>

using GubsyLobbyHostFn = bool (*)(void* user_data,
                                  const GubsyLobbyState& lobby,
                                  std::uint16_t port);
using GubsyLobbyJoinFn = bool (*)(void* user_data,
                                  const GubsyLobbyState& lobby,
                                  const char* host,
                                  std::uint16_t port);

struct GubsyLobbyCommands {
    GubsyLobbyHostFn host{nullptr};
    void* host_user_data{nullptr};
    GubsyLobbyJoinFn join{nullptr};
    void* join_user_data{nullptr};
};
