#pragma once

#include <string>

#include "engine/menu/menu_types.hpp"

MenuScreenId ensure_settings_category_screen(const std::string& tag);
const std::string* tag_for_settings_screen(MenuScreenId id);
void register_settings_category_screens();

