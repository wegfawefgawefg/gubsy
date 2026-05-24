#pragma once

#include "gubsy/lobby/state.hpp"

#include <cstdint>
#include <string>

struct GubsyLobbyHostResult {
    bool ok{false};
    std::string status;
    std::string advertised_endpoint;
};

struct GubsyLobbyJoinResult {
    bool ok{false};
    std::string status;
};

struct GubsyLobbyLeaveResult {
    bool ok{false};
    std::string status;
};

using GubsyLobbyHostFn = GubsyLobbyHostResult (*)(void* user_data,
                                                 const GubsyLobbyState& lobby,
                                                 std::uint16_t port);
using GubsyLobbyJoinFn = GubsyLobbyJoinResult (*)(void* user_data,
                                                 const GubsyLobbyState& lobby,
                                                 const char* host,
                                                 std::uint16_t port);
using GubsyLobbyLeaveFn = GubsyLobbyLeaveResult (*)(void* user_data,
                                                   const GubsyLobbyState& lobby);

struct GubsyLobbyCommands {
    GubsyLobbyHostFn host{nullptr};
    void* host_user_data{nullptr};
    GubsyLobbyJoinFn join{nullptr};
    void* join_user_data{nullptr};
    GubsyLobbyLeaveFn leave{nullptr};
    void* leave_user_data{nullptr};
};
