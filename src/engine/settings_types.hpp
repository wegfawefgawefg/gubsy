#pragma once

#include <string>
#include <variant>

struct SettingsVec2 {
    float x = 0.0f;
    float y = 0.0f;
};

using SettingsValue = std::variant<int, float, std::string, SettingsVec2>;

