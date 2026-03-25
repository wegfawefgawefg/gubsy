#pragma once

#include <string>

#include "engine/menu/menu_types.hpp"

struct EngineState;

MenuScreenId ensure_settings_category_screen(EngineState& engine, const std::string& tag);
const std::string* tag_for_settings_screen(MenuScreenId id);
void register_settings_category_screens(EngineState& engine);
