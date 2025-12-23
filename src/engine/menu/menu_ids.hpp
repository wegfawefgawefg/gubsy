#pragma once

#include "engine/menu/menu_types.hpp"

namespace MenuScreenID {
inline constexpr MenuScreenId NONE = 0;
inline constexpr MenuScreenId MODS = 1u << 20;
inline constexpr MenuScreenId SETTINGS = (1u << 20) + 1;
inline constexpr MenuScreenId SETTINGS_CATEGORY_BASE = 1u << 16;
} // namespace MenuScreenID
