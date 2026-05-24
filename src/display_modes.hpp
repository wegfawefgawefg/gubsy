#pragma once

#include "gubsy/settings/schema.hpp"

#include <SDL.h>
#include <string>
#include <vector>

struct EngineState;

std::vector<SettingOption> fullscreen_display_mode_options(EngineState& engine);
std::string configured_fullscreen_display_mode(const EngineState& engine);
bool apply_fullscreen_display_mode(SDL_Window* window, const std::string& value,
                                   std::string& applied_value);
