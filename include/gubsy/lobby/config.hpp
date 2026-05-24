#pragma once

#include "gubsy/lobby/state.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct GubsyLobbyOption {
    std::string id;
    std::string label;
    std::string description;
};

struct GubsyLobbyConfigRow {
    std::string key;
    std::string label;
    std::string description;
    std::vector<GubsyLobbyOption> options;
    int selected_option{0};
    int player_index{-1};
    bool host_only{true};
};

using GubsyLobbyConfigEnsureFn = void (*)(void* user_data, GubsyLobbyState& lobby);
using GubsyLobbyConfigRowsFn = void (*)(void* user_data, const GubsyLobbyState& lobby,
                                        std::vector<GubsyLobbyConfigRow>& out);
using GubsyLobbyConfigSetOptionFn = bool (*)(void* user_data, GubsyLobbyState& lobby,
                                             const char* key, int player_index, int option_index);
using GubsyLobbyConfigSerializeFn = nlohmann::json (*)(void* user_data,
                                                       const GubsyLobbyState& lobby);
using GubsyLobbyConfigValidateFn = bool (*)(void* user_data, const GubsyLobbyState& lobby,
                                            std::string& message);
using GubsyLobbyConfigValidateRemoteFn = bool (*)(void* user_data, const GubsyLobbyState& lobby,
                                                  const SessionContract& remote,
                                                  std::string& message);
using GubsyLobbyConfigApplyRemoteFn = bool (*)(void* user_data, GubsyLobbyState& lobby,
                                               const SessionContract& remote, std::string& message);

struct GubsyLobbyConfigProvider {
    void* user_data{nullptr};
    GubsyLobbyConfigEnsureFn ensure_defaults{nullptr};
    GubsyLobbyConfigRowsFn build_rows{nullptr};
    GubsyLobbyConfigSetOptionFn set_option{nullptr};
    GubsyLobbyConfigSerializeFn serialize{nullptr};
    GubsyLobbyConfigValidateFn validate{nullptr};
    GubsyLobbyConfigValidateRemoteFn validate_remote{nullptr};
    GubsyLobbyConfigApplyRemoteFn apply_remote{nullptr};
};
