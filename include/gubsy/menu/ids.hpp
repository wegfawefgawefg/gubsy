#pragma once

#include "gubsy/menu/types.hpp"

namespace MenuScreenID {
inline constexpr MenuScreenId NONE = 0;
inline constexpr MenuScreenId SHELL_MAIN = 1u << 19;
inline constexpr MenuScreenId SHELL_LOBBY = (1u << 19) + 1;
inline constexpr MenuScreenId IN_GAME_MENU = (1u << 19) + 11;
inline constexpr MenuScreenId LOBBY_LOCAL_PLAYERS = (1u << 19) + 2;
inline constexpr MenuScreenId LOBBY_PLAYER_SETTINGS = (1u << 19) + 3;
inline constexpr MenuScreenId LOBBY_PROFILE_PICKER = (1u << 19) + 4;
inline constexpr MenuScreenId LOBBY_BINDS_PICKER = (1u << 19) + 5;
inline constexpr MenuScreenId LOBBY_INPUT_SETTINGS_PICKER = (1u << 19) + 6;
inline constexpr MenuScreenId LOBBY_DEVICE_PICKER = (1u << 19) + 7;
inline constexpr MenuScreenId LOBBY_SERVER_BROWSER = (1u << 19) + 8;
inline constexpr MenuScreenId LOBBY_HOST_SETUP = (1u << 19) + 9;
inline constexpr MenuScreenId LOBBY_GAME_CONFIG = (1u << 19) + 10;
inline constexpr MenuScreenId LOBBY_JOIN_GAME = (1u << 19) + 12;
inline constexpr MenuScreenId LOBBY_JOIN_BY_IP = (1u << 19) + 13;
inline constexpr MenuScreenId LOBBY_REMOTE_PLAYER = (1u << 19) + 14;
inline constexpr MenuScreenId MODS = 1u << 20;
inline constexpr MenuScreenId SETTINGS = (1u << 20) + 1;
inline constexpr MenuScreenId PROFILES = (1u << 20) + 2;
inline constexpr MenuScreenId BINDS_PROFILES = (1u << 20) + 3;
inline constexpr MenuScreenId BINDS_PROFILE_EDITOR = (1u << 20) + 4;
inline constexpr MenuScreenId BINDS_ACTION_EDITOR = (1u << 20) + 5;
inline constexpr MenuScreenId BINDS_CHOOSE_INPUT = (1u << 20) + 6;
inline constexpr MenuScreenId SETTINGS_CATEGORY_BASE = 1u << 16;
} // namespace MenuScreenID
