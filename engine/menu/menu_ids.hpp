#pragma once

#include "engine/menu/menu_types.hpp"

namespace MenuScreenID {
inline constexpr MenuScreenId NONE = 0;
inline constexpr MenuScreenId MODS = 1u << 20;
inline constexpr MenuScreenId SETTINGS = (1u << 20) + 1;
inline constexpr MenuScreenId PROFILES = (1u << 20) + 2;
inline constexpr MenuScreenId BINDS_PROFILES = (1u << 20) + 3;
inline constexpr MenuScreenId BINDS_PROFILE_EDITOR = (1u << 20) + 4;
inline constexpr MenuScreenId BINDS_ACTION_EDITOR = (1u << 20) + 5;
inline constexpr MenuScreenId BINDS_CHOOSE_INPUT = (1u << 20) + 6;
inline constexpr MenuScreenId SETTINGS_CATEGORY_BASE = 1u << 16;
} // namespace MenuScreenID
