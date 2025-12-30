#pragma once

#include <string>
#include <vector>

enum class BindsActionType {
    Button = 0,
    Analog1D = 1,
    Analog2D = 2
};

struct InputChoice {
    int code{0};
    const char* label{nullptr};
};

const std::vector<InputChoice>& binds_input_choices(BindsActionType type);
std::string binds_input_label(BindsActionType type, int code);
const char* binds_action_type_label(BindsActionType type);
